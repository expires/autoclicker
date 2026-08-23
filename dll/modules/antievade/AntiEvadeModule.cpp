#include "AntiEvadeModule.h"
#include "../../config/Settings.h"
#include "../../SDK/Minecraft.h"
#include "../autoclicker/AutoclickerModule.h"
#include "../ModuleCommon.h"
#include "../../logger/Logger.h"
#include "../../overlay/Overlay.h"
#include "Mappings.h"
#include "Platform.h"
#include <atomic>
#include <cctype>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace AntiEvadeModule
{
    using Clock   = std::chrono::steady_clock;
    using Instant = Clock::time_point;

    static constexpr int  kHoldBaseMs     = 800;
    static constexpr int  kMaxPingMs      = 800;
    static constexpr auto kEvadeCooldown  = std::chrono::seconds(18);
    static constexpr auto kEntryTtl       = std::chrono::seconds(18);
    static constexpr auto kPollInterval   = std::chrono::milliseconds(5);
    static constexpr auto kExpiryInterval = std::chrono::seconds(1);
    static constexpr auto kHoverGrace     = std::chrono::milliseconds(250);
    static constexpr auto kChatInterval   = std::chrono::milliseconds(100);

    static constexpr int  kChatScan       = 8;
    static constexpr const char* kEvadeMarker = "used evade";

    static constexpr double kTrackRadiusSq = 8.0 * 8.0;
    static constexpr double kReachRadiusSq = 4.5 * 4.5;

    static constexpr size_t kNoTrack = (size_t)-1;

    struct PlayerState
    {
        Instant blockStarted{};
        Instant holdUntil{};
        Instant lastSeen{};
        bool    blocking       = false;
        bool    handled        = false;
        bool    holdingClicks  = false;
        Instant cooldownUntil{};
        bool    evadedThisBlock = false;
        bool    blockHadEvade   = false;
        int     endPingMs       = 0;
    };

    struct Track
    {
        jobject     ref = nullptr;
        std::string uuid;
        std::string name;
        bool        seen   = false;
        double      distSq = 0.0;
    };

    struct Observation
    {
        size_t track;
        bool   blocking;
        int    pingMs;
        float  health;
    };

    static std::mutex                                    s_mutex;
    static std::unordered_map<std::string, PlayerState>  s_states;
    static std::atomic<bool>                             s_hold{false};

    static std::vector<Track>       s_tracks;
    static std::vector<Observation> s_observations;

    static std::string s_lastHovered;
    static Instant     s_lastHoveredAt{};

    static int         s_lastChatTime = 0;
    static std::string s_lastChatText;
    static bool        s_chatPrimed   = false;
    static Instant     s_lastChatPoll{};

    static HWND s_window         = nullptr;
    static bool s_buttonReleased = false;

    static bool HoldingSword(LivingEntity& entity)
    {
        ItemStack held = entity.getItemInHand();
        if (held.GetInstance() == nullptr || held.isEmpty()) return false;

        std::string id = held.getDescriptionId();
        if (id.empty()) {
            Component name = held.getHoverName();
            if (name.GetInstance() == nullptr) return false;
            id = name.getString();
        }
        for (char& c : id) c = (char)std::tolower((unsigned char)c);
        return id.find("sword") != std::string::npos;
    }

    static int ArmorTierOf(const std::string& lower)
    {
        if (lower.find("netherite") != std::string::npos) return 7;
        if (lower.find("diamond")   != std::string::npos) return 6;
        if (lower.find("iron")      != std::string::npos) return 5;
        if (lower.find("turtle")    != std::string::npos) return 4;
        if (lower.find("chain")     != std::string::npos) return 3;
        if (lower.find("gold")      != std::string::npos) return 2;
        if (lower.find("leather")   != std::string::npos) return 1;
        if (lower.find("cloth")     != std::string::npos) return 1;
        return 0;
    }

    static int SlotArmorTier(LivingEntity& e, int slot)
    {
        ItemStack piece = e.getArmorItem(slot);
        if (piece.GetInstance() == nullptr || piece.isEmpty()) return 0;

        std::string id = piece.getDescriptionId();
        if (!id.empty()) {
            for (char& c : id) c = (char)std::tolower((unsigned char)c);
            return ArmorTierOf(id);
        }

        Component hn = piece.getHoverName();
        if (hn.GetInstance() == nullptr) return 0;
        std::string s = hn.getString();
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return ArmorTierOf(s);
    }

    static bool ArmorQualifies(LivingEntity& e)
    {
        int tier = SlotArmorTier(e, 1);
        for (int slot : { 2, 0, 3 }) {
            const int t = SlotArmorTier(e, slot);
            if (t > tier) tier = t;
        }
        return tier == 1 || tier == 3;
    }

    static size_t FindTrack(jobject inst)
    {
        for (size_t i = 0; i < s_tracks.size(); ++i)
            if (lc->env->IsSameObject(s_tracks[i].ref, inst) == JNI_TRUE) return i;
        return kNoTrack;
    }

    static size_t AcquireTrack(Entity& entity)
    {
        jobject inst = entity.GetInstance();

        const size_t existing = FindTrack(inst);
        if (existing != kNoTrack) return existing;

        std::string uuid = entity.getUUID();
        if (uuid.empty()) return kNoTrack;

        std::string name;
        Component named = entity.getName();
        if (named.GetInstance() != nullptr) {
            name = named.getString();
            for (char& c : name) c = (char)std::tolower((unsigned char)c);
        }

        jobject ref = lc->env->NewGlobalRef(inst);
        if (ref == nullptr) { lc->env->ExceptionClear(); return kNoTrack; }

        s_tracks.push_back(Track{ ref, std::move(uuid), std::move(name), false, 0.0 });
        return s_tracks.size() - 1;
    }

    static void PurgeUnseenTracks()
    {
        for (size_t i = 0; i < s_tracks.size(); ) {
            if (s_tracks[i].seen) { ++i; continue; }
            lc->env->DeleteGlobalRef(s_tracks[i].ref);
            if (i != s_tracks.size() - 1) s_tracks[i] = std::move(s_tracks.back());
            s_tracks.pop_back();
        }
    }

    static void ClearTracks()
    {
        for (auto& t : s_tracks)
            if (t.ref != nullptr) lc->env->DeleteGlobalRef(t.ref);
        s_tracks.clear();
    }

    static void Expire(Instant now)
    {
        for (auto it = s_states.begin(); it != s_states.end(); ) {
            if (now - it->second.lastSeen > kEntryTtl) it = s_states.erase(it);
            else                                       ++it;
        }
    }

    static void ApplyObservation(const Track& track, bool blocking, Instant now, int pingMs, float health)
    {
        PlayerState& st = s_states[track.uuid];
        st.lastSeen = now;

        if (st.cooldownUntil > now && health > -0.5f && health <= 0.0f)
            st.cooldownUntil = now;

        const bool onCooldown = now < st.cooldownUntil;

        if (blocking && !st.blocking) {
            st.blocking        = true;
            st.blockStarted    = now;
            st.handled         = false;
            st.evadedThisBlock = false;
            st.blockHadEvade   = !onCooldown;
        }
        else if (!blocking && st.blocking) {
            st.blocking = false;
            if (st.blockHadEvade && !st.evadedThisBlock)
                st.cooldownUntil = now + kEvadeCooldown;

            if (st.handled) {
                const Instant drain = now + std::chrono::milliseconds(st.endPingMs);
                if (drain < st.holdUntil) st.holdUntil = drain;
            }
        }

        if (blocking && st.blockHadEvade && !st.handled) {
            st.handled   = true;
            st.endPingMs = pingMs;
            st.holdUntil = now + std::chrono::milliseconds(kHoldBaseMs)
                             + std::chrono::milliseconds(pingMs);
        }

        st.holdingClicks = st.handled && now < st.holdUntil;
    }

    static void RearmAfterEvade(PlayerState& st, Instant now)
    {
        st.handled         = false;
        st.blockStarted    = now;
        st.lastSeen        = now;
        st.cooldownUntil   = now;
        st.evadedThisBlock = true;
        st.blockHadEvade   = true;
    }

    static void CollectEvadeMessages(Minecraft& mc, Instant now, std::vector<size_t>& evaded)
    {
        if (now - s_lastChatPoll < kChatInterval) return;
        s_lastChatPoll = now;

        Gui gui = mc.GetGui();
        if (gui.GetInstance() == nullptr) return;

        ChatComponent chat = gui.getChat();
        if (chat.GetInstance() == nullptr) return;

        std::vector<ChatMessage> messages = chat.getRecentMessages(kChatScan);
        if (messages.empty()) return;

        const int  newest = messages.front().addedTime;
        const bool timed  = newest != 0;

        if (timed && newest < s_lastChatTime) s_chatPrimed = false;

        if (!s_chatPrimed) {
            s_chatPrimed   = true;
            s_lastChatTime = newest;
            s_lastChatText = messages.front().text;
            return;
        }

        size_t fresh = 0;
        if (timed) {
            while (fresh < messages.size() && messages[fresh].addedTime > s_lastChatTime) ++fresh;
        }
        else {
            while (fresh < messages.size() && messages[fresh].text != s_lastChatText) ++fresh;
        }

        s_lastChatTime = newest;
        s_lastChatText = messages.front().text;

        for (size_t m = 0; m < fresh; ++m) {
            std::string line = messages[m].text;
            for (char& c : line) c = (char)std::tolower((unsigned char)c);
            if (line.find(kEvadeMarker) == std::string::npos) continue;

            for (size_t i = 0; i < s_tracks.size(); ++i) {
                if (s_tracks[i].name.empty()) continue;
                if (line.find(s_tracks[i].name) != std::string::npos) {
                    evaded.push_back(i);
                    break;
                }
            }
        }
    }

    static bool GameHasFocus()
    {
        return s_window != nullptr
            && GetForegroundWindow() == s_window
            && !Overlay::IsMenuVisible();
    }

    static void ApplyButtonState(bool hold)
    {
        if (s_window == nullptr) s_window = FindGameWindow();
        if (s_window == nullptr) return;
        if (hold == s_buttonReleased) return;

        const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

        POINT pt;
        GetCursorPos(&pt);

        if (hold) {
            if (!lmb || !GameHasFocus()) return;
            SendMessageW(s_window, WM_LBUTTONUP, 0, MAKELPARAM(pt.x, pt.y));
            s_buttonReleased = true;
            return;
        }

        s_buttonReleased = false;
        if (!lmb || !GameHasFocus()) return;

        SendMessageW(s_window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));
    }

    static void RestoreButton()
    {
        if (!s_buttonReleased) return;
        s_buttonReleased = false;
        if (s_window == nullptr) return;
        if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) return;

        POINT pt;
        GetCursorPos(&pt);
        SendMessageW(s_window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));
    }

    static Entity HoveredEntity(Minecraft& mc)
    {
        HitResult hr = mc.getHitResult();
        if (hr.GetInstance() == nullptr || hr.getType() != 2) return Entity(nullptr);

        EntityHitResult ehr = hr.getEntityHitResult();
        if (ehr.GetInstance() == nullptr) return Entity(nullptr);

        return ehr.getEntity();
    }

    static void Tick(Minecraft& mc, Instant now)
    {
        JLocalFrame frame(2048);
        if (!frame.ok()) return;

        Player local = mc.GetLocalPlayer();
        if (local.GetInstance() == nullptr) return;

        Level level = mc.GetLevel();
        if (level.GetInstance() == nullptr) return;

        const double lx = local.getX();
        const double ly = local.getY();
        const double lz = local.getZ();

        const int lping = local.getLatency();

        jobject localInst = local.GetInstance();

        for (auto& t : s_tracks) t.seen = false;

        s_observations.clear();

        for (auto& p : level.players())
        {
            if (lc->env->IsSameObject(p.GetInstance(), localInst) == JNI_TRUE) continue;

            const double dx = p.getX() - lx;
            const double dy = p.getY() - ly;
            const double dz = p.getZ() - lz;

            const double distSq = dx * dx + dy * dy + dz * dz;
            if (distSq > kTrackRadiusSq) continue;

            const size_t index = AcquireTrack(p);
            if (index == kNoTrack) continue;

            Track& track = s_tracks[index];
            track.seen   = true;
            track.distSq = distSq;

            LivingEntity living(p.GetInstance());
            bool blocking = living.isUsingItem() && HoldingSword(living);
            if (blocking && !ArmorQualifies(living)) blocking = false;

            int pingMs = 0;
            if (blocking) {
                const int lp = lping > 0 ? lping : 0;
                const int vp = living.getLatency();
                pingMs = (lp + (vp > 0 ? vp : 0)) / 2;
                if (pingMs > kMaxPingMs) pingMs = kMaxPingMs;
            }

            const float health = living.getHealth();

            s_observations.push_back(Observation{ index, blocking, pingMs, health });
        }

        size_t hovered = kNoTrack;
        {
            Entity target = HoveredEntity(mc);
            if (target.GetInstance() != nullptr)
                hovered = FindTrack(target.GetInstance());
        }

        std::vector<size_t> evaded;
        CollectEvadeMessages(mc, now, evaded);

        std::string targetUuid;
        if (hovered != kNoTrack) {
            targetUuid      = s_tracks[hovered].uuid;
            s_lastHovered   = targetUuid;
            s_lastHoveredAt = now;
        }
        else if (!s_lastHovered.empty() && now - s_lastHoveredAt < kHoverGrace) {
            targetUuid = s_lastHovered;
        }

        bool hold = false;
        {
            std::lock_guard<std::mutex> lk(s_mutex);

            static Instant lastExpiry{};
            if (now - lastExpiry > kExpiryInterval) {
                lastExpiry = now;
                Expire(now);
            }

            for (size_t index : evaded) {
                auto it = s_states.find(s_tracks[index].uuid);
                if (it != s_states.end()) RearmAfterEvade(it->second, now);
            }

            for (const Observation& ob : s_observations)
                ApplyObservation(s_tracks[ob.track], ob.blocking, now, ob.pingMs, ob.health);

            if (!targetUuid.empty()) {
                auto it = s_states.find(targetUuid);
                if (it != s_states.end()) hold = it->second.holdingClicks;
            }
            else {
                for (const Track& t : s_tracks) {
                    if (!t.seen || t.distSq > kReachRadiusSq) continue;
                    auto it = s_states.find(t.uuid);
                    if (it == s_states.end() || !it->second.holdingClicks) continue;
                    hold = true;
                    break;
                }
            }
        }

        PurgeUnseenTracks();

        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

        s_hold.store(hold, std::memory_order_relaxed);
        ApplyButtonState(hold);
    }

    bool ShouldHoldClicks()
    {
        return g_settings.antiEvadeEnabled && s_hold.load(std::memory_order_relaxed);
    }

    DWORD WINAPI init(LPVOID)
    {
        LOG("antievade: thread start");
        if (!ModuleCommon::AttachToJvm()) return 0;
        LOG("antievade: attached; entering loop");

        Minecraft mc;
        bool wasEnabled = false;

        while (!AutoclickerModule::destruct)
        {
            std::this_thread::sleep_for(kPollInterval);

            if (!g_settings.antiEvadeEnabled) {
                if (wasEnabled) {
                    wasEnabled = false;
                    s_hold.store(false, std::memory_order_relaxed);
                    RestoreButton();
                    ClearTracks();
                    s_observations.clear();
                    s_lastHovered.clear();
                    s_lastChatTime = 0;
                    s_lastChatText.clear();
                    s_chatPrimed   = false;
                    std::lock_guard<std::mutex> lk(s_mutex);
                    s_states.clear();
                }
                continue;
            }
            wasEnabled = true;

            if (mc.GetInstance() == nullptr) {
                if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                ClearTracks();
                continue;
            }

            Tick(mc, Clock::now());
        }

        LOG("antievade: loop exit; detaching");
        RestoreButton();
        ClearTracks();
        lc->vm->DetachCurrentThread();
        return 0;
    }
}
