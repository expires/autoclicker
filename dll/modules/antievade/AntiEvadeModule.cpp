#include "AntiEvadeModule.h"
#include "../../config/Settings.h"
#include "../../SDK/Minecraft.h"
#include "../autoclicker/AutoclickerModule.h"
#include "../ModuleCommon.h"
#include "../../logger/Logger.h"
#include "Mappings.h"
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

    static constexpr auto kBlockThreshold = std::chrono::milliseconds(50);
    static constexpr auto kHoldWindow     = std::chrono::milliseconds(700);
    static constexpr auto kEntryTtl       = std::chrono::seconds(18);
    static constexpr auto kAbilityCooldown = std::chrono::seconds(18);
    static constexpr auto kPollInterval   = std::chrono::milliseconds(5);
    static constexpr auto kExpiryInterval = std::chrono::seconds(1);
    static constexpr auto kArmorTtl       = std::chrono::milliseconds(500);
    static constexpr auto kHoverGrace     = std::chrono::milliseconds(250);
    static constexpr auto kChatInterval   = std::chrono::milliseconds(100);

    static constexpr int  kChatScan       = 8;
    static constexpr const char* kEvadeMarker = "used evade";

    static constexpr double kTrackRadiusSq = 8.0 * 8.0;

    struct PlayerState
    {
        Instant blockStarted{};
        Instant holdUntil{};
        Instant lockoutUntil{};
        bool    blocking         = false;
        bool    thresholdReached = false;
        bool    handled          = false;
        bool    holdingClicks    = false;
        bool    struckBlock      = false;
        Instant lastSeen{};
    };

    struct Track
    {
        jobject     ref = nullptr;
        std::string uuid;
        std::string name;
        bool        leather   = false;
        Instant     checkedAt{};
        bool        seen      = false;
    };

    struct Observation
    {
        size_t track;
        bool   blocking;
    };

    static std::mutex                                    s_mutex;
    static std::unordered_map<std::string, PlayerState>  s_states;
    static std::string                                   s_holdTarget;
    static std::string                                   s_activeTarget;
    static std::atomic<bool>                             s_hold{false};

    static std::vector<Track>       s_tracks;
    static std::vector<Observation> s_observations;

    static std::string s_lastHovered;
    static Instant     s_lastHoveredAt{};

    static int         s_lastChatTime = 0;
    static std::string s_lastChatText;
    static bool        s_chatPrimed   = false;
    static Instant     s_lastChatPoll{};

    static constexpr size_t kNoTrack = (size_t)-1;

    static bool NameIsLeather(const std::string& lower)
    {
        return lower.find("leather") != std::string::npos
            || lower.find("cloth")   != std::string::npos;
    }

    static bool NameIsSword(const std::string& lower)
    {
        return lower.find("sword") != std::string::npos;
    }

    static bool StackMatches(ItemStack& stack, bool (*pred)(const std::string&))
    {
        if (stack.GetInstance() == nullptr || stack.isEmpty()) return false;

        std::string id = stack.getDescriptionId();
        if (id.empty()) {
            Component name = stack.getHoverName();
            if (name.GetInstance() == nullptr) return false;
            id = name.getString();
        }
        for (char& c : id) c = (char)std::tolower((unsigned char)c);
        return pred(id);
    }

    static bool WearingLeather(LivingEntity& entity)
    {
        ItemStack chest = entity.getArmorItem(1);
        if (chest.GetInstance() != nullptr && !chest.isEmpty())
            return StackMatches(chest, NameIsLeather);

        static const int others[3] = { 2, 0, 3 };
        for (int slot : others) {
            ItemStack piece = entity.getArmorItem(slot);
            if (StackMatches(piece, NameIsLeather)) return true;
        }
        return false;
    }

    static bool HoldingSword(LivingEntity& entity)
    {
        ItemStack held = entity.getItemInHand();
        return StackMatches(held, NameIsSword);
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

        s_tracks.push_back(Track{ ref, std::move(uuid), std::move(name), false, Instant{}, false });
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
            const PlayerState& st = it->second;
            if (now - st.lastSeen > kEntryTtl && now >= st.lockoutUntil) it = s_states.erase(it);
            else                                                        ++it;
        }
    }

    static void ApplyObservation(const std::string& uuid, bool blocking, Instant now)
    {
        PlayerState& st = s_states[uuid];
        st.lastSeen = now;

        if (blocking) {
            if (!st.blocking) {
                st.blocking         = true;
                st.blockStarted     = now;
                st.thresholdReached = false;
                st.handled          = false;
                st.holdingClicks    = false;
                st.struckBlock      = false;
            }

            if (!st.handled && now - st.blockStarted > kBlockThreshold) {
                st.thresholdReached = true;
                st.handled          = true;

                if (now >= st.lockoutUntil) {
                    st.holdingClicks = true;
                    st.holdUntil     = now + kHoldWindow;
                    st.lockoutUntil  = now + kAbilityCooldown;
                }
            }

            if (st.holdingClicks && now >= st.holdUntil)
                st.holdingClicks = false;
        }
        else if (st.blocking) {
            st.blocking         = false;
            st.thresholdReached = false;
            st.handled          = false;
            st.holdingClicks    = false;
            st.struckBlock      = false;
        }
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

    static void ClearLockout(PlayerState& st, Instant now)
    {
        st.lockoutUntil = now;
        st.handled      = false;
        st.blockStarted = now;
        st.lastSeen     = now;
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

        jobject localInst = local.GetInstance();

        for (auto& t : s_tracks) t.seen = false;

        s_observations.clear();

        for (auto& p : level.players())
        {
            if (lc->env->IsSameObject(p.GetInstance(), localInst) == JNI_TRUE) continue;

            const double dx = p.getX() - lx;
            const double dy = p.getY() - ly;
            const double dz = p.getZ() - lz;
            if (dx * dx + dy * dy + dz * dz > kTrackRadiusSq) continue;

            const size_t index = AcquireTrack(p);
            if (index == kNoTrack) continue;

            Track& track = s_tracks[index];
            track.seen = true;

            LivingEntity living(p.GetInstance());

            if (now - track.checkedAt > kArmorTtl) {
                track.leather   = WearingLeather(living);
                track.checkedAt = now;
            }
            if (!track.leather) continue;

            const bool blocking = living.isUsingItem() && HoldingSword(living);
            s_observations.push_back(Observation{ index, blocking });
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

        bool        hold = false;
        std::string holdTarget;
        std::string activeTarget;
        {
            std::lock_guard<std::mutex> lk(s_mutex);

            static Instant lastExpiry{};
            if (now - lastExpiry > kExpiryInterval) {
                lastExpiry = now;
                Expire(now);
            }

            for (size_t index : evaded) {
                auto it = s_states.find(s_tracks[index].uuid);
                if (it != s_states.end()) ClearLockout(it->second, now);
            }

            for (const Observation& ob : s_observations)
                ApplyObservation(s_tracks[ob.track].uuid, ob.blocking, now);

            if (!targetUuid.empty()) {
                activeTarget = targetUuid;
                auto it = s_states.find(activeTarget);
                if (it != s_states.end()) hold = it->second.holdingClicks;
                if (hold) holdTarget = activeTarget;
            }

            s_holdTarget   = std::move(holdTarget);
            s_activeTarget = std::move(activeTarget);
        }

        PurgeUnseenTracks();

        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

        s_hold.store(hold, std::memory_order_relaxed);
    }

    bool ShouldHoldClicks()
    {
        return g_settings.antiEvadeEnabled && s_hold.load(std::memory_order_relaxed);
    }

    void NoteHit()
    {
        if (!g_settings.antiEvadeEnabled) return;

        std::lock_guard<std::mutex> lk(s_mutex);

        const std::string& uuid = s_holdTarget.empty() ? s_activeTarget : s_holdTarget;
        if (uuid.empty()) return;

        auto it = s_states.find(uuid);
        if (it == s_states.end()) return;

        it->second.lastSeen = Clock::now();
        if (it->second.blocking) it->second.struckBlock = true;
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
                    ClearTracks();
                    s_observations.clear();
                    s_lastHovered.clear();
                    s_lastChatTime = 0;
                    s_lastChatText.clear();
                    s_chatPrimed   = false;
                    std::lock_guard<std::mutex> lk(s_mutex);
                    s_states.clear();
                    s_holdTarget.clear();
                    s_activeTarget.clear();
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
        ClearTracks();
        lc->vm->DetachCurrentThread();
        return 0;
    }
}
