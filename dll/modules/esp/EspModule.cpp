#include "EspModule.h"
#include "../../config/Settings.h"
#include "../../SDK/Minecraft.h"
#include "../../SDK/ItemStack.h"
#include "../../SDK/Component.h"
#include "../../SDK/ParticleEngine.h"
#include "../../SDK/View.h"
#include "../../SDK/BlockPos.h"
#include "Mappings.h"
#include "../autoclicker/AutoclickerModule.h"
#include "../ModuleCommon.h"
#include "../../logger/Logger.h"
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace EspModule
{
    static std::mutex                       s_mutex;
    static std::shared_ptr<const Snapshot>  s_snapshot;

    std::shared_ptr<const Snapshot> Acquire()
    {
        std::lock_guard<std::mutex> lk(s_mutex);
        return s_snapshot;
    }

    static void Publish(std::shared_ptr<const Snapshot> snap)
    {
        std::lock_guard<std::mutex> lk(s_mutex);
        s_snapshot = std::move(snap);
    }

    struct NameEntry
    {
        jobject ref = nullptr;
        std::vector<std::pair<std::string, uint32_t>> chunks;
        std::string lowerName;
        uint32_t armorColor = 0u;
        std::chrono::steady_clock::time_point refreshedAt{};
        bool seen = false;
    };

    static bool blockIsTorch(Level& level, int bx, int by, int bz)
    {
        BlockPos bp = BlockPos::make(bx, by, bz);
        jobject bpInst = bp.GetInstance();
        if (bpInst == nullptr) return false;

        BlockState bs = level.getBlockState(bp);
        jobject bsInst = bs.GetInstance();
        if (bsInst == nullptr) { lc->env->DeleteLocalRef(bpInst); return false; }

        static jmethodID toStr = nullptr;
        if (toStr == nullptr) {
            jclass oc = lc->env->FindClass("java/lang/Object");
            if (oc) { toStr = lc->env->GetMethodID(oc, "toString", "()Ljava/lang/String;"); lc->env->DeleteLocalRef(oc); }
            if (toStr == nullptr) lc->env->ExceptionClear();
        }

        bool hit = false;
        if (toStr) {
            jstring js = (jstring)lc->env->CallObjectMethod(bsInst, toStr);
            if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); }
            else if (js) {
                const char* c = lc->env->GetStringUTFChars(js, nullptr);
                if (c) {
                    hit = (std::strstr(c, "torch") != nullptr) || (std::strstr(c, "Torch") != nullptr);
                    lc->env->ReleaseStringUTFChars(js, c);
                }
            }
            if (js) lc->env->DeleteLocalRef(js);
        }

        lc->env->DeleteLocalRef(bsInst);
        lc->env->DeleteLocalRef(bpInst);
        return hit;
    }

    struct PlayerTrack { double x, y, z, vx, vy, vz; };

    struct VanishEvent
    {
        double x, y, z, vx, vy, vz;
        std::string ign;
        std::chrono::steady_clock::time_point at;
    };

    static constexpr uint32_t packColor(uint32_t r, uint32_t g, uint32_t b)
    {
        return (0xFFu << 24) | (b << 16) | (g << 8) | r;
    }

    static int armorTier(const std::string& lower)
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

    static uint32_t armorTierColor(int tier)
    {
        switch (tier) {
            case 7: return packColor( 70,  62,  66);
            case 6: return packColor( 85, 200, 255);
            case 5: return packColor(225, 225, 230);
            case 4: return packColor( 90, 180, 100);
            case 3: return packColor(170, 170, 175);
            case 2: return packColor(250, 210,  60);
            case 1: return packColor(150, 100,  60);
            default: return 0u;
        }
    }

    static int slotArmorTier(Player& p, int slot)
    {
        ItemStack piece = p.getArmorItem(slot);
        if (piece.GetInstance() == nullptr || piece.isEmpty()) return 0;

        std::string id = piece.getDescriptionId();
        if (!id.empty()) {
            for (char& c : id) c = (char)std::tolower((unsigned char)c);
            return armorTier(id);
        }

        Component hn = piece.getHoverName();
        if (hn.GetInstance() == nullptr) return 0;
        std::string s = hn.getString();
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return armorTier(s);
    }

    static uint32_t scanArmorColor(Player& p)
    {
        int tier = slotArmorTier(p, 1);
        if (tier == 0) {
            static const int others[3] = { 2, 0, 3 };
            for (int slot : others) {
                const int t = slotArmorTier(p, slot);
                if (t > tier) tier = t;
            }
        }
        return armorTierColor(tier);
    }

    static void purgeNames(std::vector<NameEntry>& cache)
    {
        for (auto& e : cache)
            if (e.ref) lc->env->DeleteGlobalRef(e.ref);
        cache.clear();
    }

    static void refreshName(NameEntry& e, Player& p,
                            std::chrono::steady_clock::time_point now)
    {
        e.chunks = p.getFormattedNameChunks();
        e.lowerName.clear();
        Component bare = p.getName();
        if (bare.GetInstance() != nullptr) {
            e.lowerName = bare.getString();
            for (char& c : e.lowerName)
                c = (char)std::tolower((unsigned char)c);
        }
        e.armorColor = g_settings.drawArmor ? scanArmorColor(p) : 0u;
        e.refreshedAt = now;
    }

    static const NameEntry& lookupName(std::vector<NameEntry>& cache, size_t hint,
                                       Player& p, std::chrono::steady_clock::time_point now)
    {
        constexpr auto TTL = std::chrono::milliseconds(100);

        size_t found = cache.size();
        if (hint < cache.size() &&
            lc->env->IsSameObject(cache[hint].ref, p.GetInstance()) == JNI_TRUE) {
            found = hint;
        } else {
            for (size_t i = 0; i < cache.size(); ++i) {
                if (i == hint) continue;
                if (lc->env->IsSameObject(cache[i].ref, p.GetInstance()) == JNI_TRUE) {
                    found = i;
                    break;
                }
            }
        }

        if (found == cache.size()) {
            NameEntry e;
            e.ref  = lc->env->NewGlobalRef(p.GetInstance());
            e.seen = true;
            refreshName(e, p, now);
            cache.push_back(std::move(e));
        } else {
            NameEntry& e = cache[found];
            e.seen = true;
            if (now - e.refreshedAt >= TTL)
                refreshName(e, p, now);
        }

        if (hint < cache.size() && found < cache.size() && found != hint) {
            std::swap(cache[hint], cache[found]);
            found = hint;
        }
        return cache[found];
    }

    DWORD WINAPI init(LPVOID lpParam)
    {
        LOG("esp: thread start");
        if (!ModuleCommon::AttachToJvm()) return 0;
        LOG("esp: attached; entering loop");

        Minecraft mc;
        bool clearedWhileDisabled = false;
        std::vector<NameEntry> nameCache;
        std::unordered_map<std::string, PlayerTrack> tracks;
        std::vector<VanishEvent> vanishEvents;
        std::chrono::steady_clock::time_point lastFullScan{};

        while (!AutoclickerModule::destruct)
        {
            if (!g_settings.espEnabled && !g_settings.smokeEspEnabled)
            {
                if (!clearedWhileDisabled)
                {
                    purgeNames(nameCache);
                    Publish(std::make_shared<Snapshot>());
                    clearedWhileDisabled = true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
            clearedWhileDisabled = false;

            if (lc->env->PushLocalFrame(2048) != 0)
            {
                lc->env->ExceptionClear();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            auto back = std::make_shared<Snapshot>();

            auto publishDiag = [&]() { Publish(back); };

            jobject mcInst = mc.GetInstance();
            if (mcInst == nullptr) { lc->env->PopLocalFrame(nullptr); purgeNames(nameCache); publishDiag(); std::this_thread::yield(); continue; }
            back->gotMinecraft = true;

            Player localPlayer = mc.GetLocalPlayer();
            if (localPlayer.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); purgeNames(nameCache); publishDiag(); std::this_thread::yield(); continue; }
            back->gotLocalPlayer = true;

            Level level = mc.GetLevel();
            if (level.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); purgeNames(nameCache); publishDiag(); std::this_thread::yield(); continue; }
            back->gotLevel = true;

            ViewState view = AcquireView(mc, localPlayer);
            back->gotGameRenderer = view.gotRenderer;
            back->gotCamera       = view.gotCamera;
            if (!view.ok) { lc->env->PopLocalFrame(nullptr); publishDiag(); std::this_thread::yield(); continue; }

            back->partialTick = view.partialTick;
            back->cam.x    = view.x;
            back->cam.y    = view.y;
            back->cam.z    = view.z;
            back->cam.yRot = view.yRot;
            back->cam.xRot = view.xRot;
            back->cam.fov  = view.fov;

            auto players = level.players();
            back->rawPlayerCount = (int)players.size();
            jobject localInst = localPlayer.GetInstance();
            const double maxDistSq =
                (double)g_settings.maxDistance * (double)g_settings.maxDistance;

            back->targets.reserve(players.size());

            std::vector<std::string> friendsSnapshot;
            {
                std::lock_guard<std::mutex> lk(g_settings.friendsMutex);
                friendsSnapshot = g_settings.friends;
            }

            const auto scanNow = std::chrono::steady_clock::now();
            for (auto& e : nameCache) e.seen = false;
            size_t cacheHint = 0;

            std::unordered_map<std::string, PlayerTrack> present;

            for (auto& p : players)
            {
                if (lc->env->IsSameObject(p.GetInstance(), localInst)) continue;

                Target t;
                Vec3 pos = p.getPosition();
                if (pos.GetInstance() == nullptr) continue;
                t.x = pos.getX();
                t.y = pos.getY();
                t.z = pos.getZ();
                t.prevX = p.getXo();
                t.prevY = p.getYo();
                t.prevZ = p.getZo();

                double dx = t.x - back->cam.x, dy = t.y - back->cam.y, dz = t.z - back->cam.z;
                double distSq = dx*dx + dy*dy + dz*dz;
                if (distSq > maxDistSq) continue;

                AABB box = p.getBoundingBox();
                if (box.GetInstance() != nullptr)
                {
                    t.halfWidth = (box.maxX() - box.minX()) * 0.5;
                    t.height    =  box.maxY() - box.minY();
                    t.halfDepth = (box.maxZ() - box.minZ()) * 0.5;
                }
                else
                {
                    t.halfWidth = 0.3;
                    t.height    = 1.8;
                    t.halfDepth = 0.3;
                }

                {
                    const NameEntry& ni = lookupName(nameCache, cacheHint++, p, scanNow);
                    t.nameChunks = ni.chunks;
                    t.playerName = ni.lowerName;
                    t.armorColor = ni.armorColor;
                }

                t.health    = p.getHealth();
                t.maxHealth = p.getMaxHealth();

                if (!t.playerName.empty()) {
                    for (const auto& f : friendsSnapshot) {
                        if (f == t.playerName) { t.isFriend = true; break; }
                    }
                }

                t.boxColor = 0xFFFFFFFFu;
                if (g_settings.teamsByColor) {
                    for (auto it = t.nameChunks.rbegin(); it != t.nameChunks.rend(); ++it) {
                        if (!it->first.empty()) { t.boxColor = it->second; break; }
                    }
                }

                if (g_settings.smokeEspEnabled && !t.playerName.empty()) {
                    present[t.playerName] = PlayerTrack{
                        t.x, t.y, t.z,
                        t.x - t.prevX, t.y - t.prevY, t.z - t.prevZ
                    };
                }

                back->targets.push_back(std::move(t));
            }

            if (g_settings.smokeEspEnabled) {
                const double gapMs =
                    (lastFullScan.time_since_epoch().count() == 0)
                        ? 1e9
                        : std::chrono::duration<double, std::milli>(scanNow - lastFullScan).count();
                const bool freshSequence = gapMs > 250.0;
                lastFullScan = scanNow;

                std::vector<const std::pair<const std::string, PlayerTrack>*> gone;
                for (const auto& kv : tracks)
                    if (present.find(kv.first) == present.end())
                        gone.push_back(&kv);

                if (!freshSequence && gone.size() <= 3) {
                    const double innerR =
                        (g_settings.maxDistance > 8) ? (double)(g_settings.maxDistance - 8)
                                                     : (double)g_settings.maxDistance;
                    const double innerRSq = innerR * innerR;
                    for (auto* g : gone) {
                        const PlayerTrack& tr = g->second;
                        const double dx = tr.x - back->cam.x;
                        const double dy = tr.y - back->cam.y;
                        const double dz = tr.z - back->cam.z;
                        if (dx*dx + dy*dy + dz*dz <= innerRSq)
                            vanishEvents.push_back(
                                VanishEvent{ tr.x, tr.y, tr.z, tr.vx, tr.vy, tr.vz, g->first, scanNow });
                    }
                }

                tracks = std::move(present);

                for (size_t i = 0; i < vanishEvents.size(); ) {
                    const double ageMs =
                        std::chrono::duration<double, std::milli>(scanNow - vanishEvents[i].at).count();
                    if (ageMs > 2500.0) {
                        vanishEvents[i] = std::move(vanishEvents.back());
                        vanishEvents.pop_back();
                    } else {
                        ++i;
                    }
                }

                back->vanishes.reserve(vanishEvents.size());
                for (const auto& v : vanishEvents) {
                    const double ageMs =
                        std::chrono::duration<double, std::milli>(scanNow - v.at).count();
                    const double ticks = ageMs / 50.0;
                    double ex = v.vx * ticks, ey = v.vy * ticks, ez = v.vz * ticks;
                    const double disp = std::sqrt(ex*ex + ey*ey + ez*ez);
                    if (disp > 6.0 && disp > 1e-6) {
                        const double s = 6.0 / disp;
                        ex *= s; ey *= s; ez *= s;
                    }
                    back->vanishes.push_back(
                        VanishMark{ v.x + ex, v.y + ey, v.z + ez, v.ign, (float)ageMs });
                }
            } else {
                tracks.clear();
                vanishEvents.clear();
            }

            if (g_settings.smokeEspEnabled && Particles::Supported()) {
                std::vector<Particles::Point> pts;
                Particles::CollectSmoke(pts,
                                        back->cam.x, back->cam.y, back->cam.z,
                                        (double)g_settings.maxDistance, 600);

                auto packBlock = [](int x, int y, int z) -> long long {
                    return ((long long)(x & 0x1FFFFF))
                         | ((long long)(y & 0x1FFFFF) << 21)
                         | ((long long)(z & 0x1FFFFF) << 42);
                };
                std::unordered_map<long long, char> torchCache;
                auto isTorchAt = [&](int x, int y, int z) -> bool {
                    const long long k = packBlock(x, y, z);
                    auto it = torchCache.find(k);
                    if (it != torchCache.end()) return it->second == 1;
                    const bool t = blockIsTorch(level, x, y, z);
                    torchCache[k] = t ? 1 : 2;
                    return t;
                };

                back->smoke.reserve(pts.size());
                for (const auto& pp : pts) {
                    const int bx = (int)std::floor(pp.x);
                    const int by = (int)std::floor(pp.y);
                    const int bz = (int)std::floor(pp.z);
                    bool torch = false;
                    for (int dy = 0; dy <= 2 && !torch; ++dy)
                        if (isTorchAt(bx, by - dy, bz)) torch = true;
                    if (!torch)
                        back->smoke.push_back({ pp.x, pp.y, pp.z });
                }
            }

            for (size_t i = 0; i < nameCache.size(); ) {
                if (!nameCache[i].seen) {
                    if (nameCache[i].ref) lc->env->DeleteGlobalRef(nameCache[i].ref);
                    nameCache[i] = std::move(nameCache.back());
                    nameCache.pop_back();
                } else {
                    ++i;
                }
            }

            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

            back->valid = true;

            lc->env->PopLocalFrame(nullptr);

            Publish(back);

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        LOG("esp: loop exit; detaching");
        purgeNames(nameCache);
        lc->vm->DetachCurrentThread();
        return 0;
    }
}
