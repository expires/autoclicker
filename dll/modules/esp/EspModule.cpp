#include "EspModule.h"
#include "../../config/Settings.h"
#include "../../SDK/Minecraft.h"
#include "../../SDK/View.h"
#include "Mappings.h"
#include "../autoclicker/AutoclickerModule.h"
#include "../ModuleCommon.h"
#include "../../logger/Logger.h"
#include <cctype>
#include <chrono>
#include <mutex>
#include <thread>

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
        std::chrono::steady_clock::time_point refreshedAt{};
        bool seen = false;
    };

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

        while (!AutoclickerModule::destruct)
        {
            if (!g_settings.espEnabled)
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

                back->targets.push_back(std::move(t));
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
