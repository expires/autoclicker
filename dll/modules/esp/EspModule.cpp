#include "EspModule.h"
#include "../../config/Settings.h"
#include "../../SDK/Minecraft.h"
#include "Mappings.h"
#include "../autoclicker/AutoclickerModule.h"
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

    static bool jvmReady()
    {
        return lc->vm != nullptr && lc->classesLoaded.load(std::memory_order_acquire);
    }

    DWORD WINAPI init(LPVOID lpParam)
    {
        AC_LOG("esp: thread start");
        while (!AutoclickerModule::destruct && !jvmReady())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (AutoclickerModule::destruct) return 0;

        if (lc->vm->AttachCurrentThread(reinterpret_cast<void**>(&lc->env), nullptr) != JNI_OK)
            return 0;
        if (lc->env == nullptr) return 0;
        AC_LOG("esp: attached; entering loop");

        Minecraft mc;

        while (!AutoclickerModule::destruct)
        {
            if (!g_settings.espEnabled)
            {
                Publish(std::make_shared<Snapshot>());
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            if (lc->env->PushLocalFrame(2048) != 0)
            {
                lc->env->ExceptionClear();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            auto back = std::make_shared<Snapshot>();

            auto publishDiag = [&]() { Publish(back); };

            jobject mcInst = mc.GetInstance();
            if (mcInst == nullptr) { lc->env->PopLocalFrame(nullptr); publishDiag(); std::this_thread::yield(); continue; }
            back->gotMinecraft = true;

            Player localPlayer = mc.GetLocalPlayer();
            if (localPlayer.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); publishDiag(); std::this_thread::yield(); continue; }
            back->gotLocalPlayer = true;

            Level level = mc.GetLevel();
            if (level.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); publishDiag(); std::this_thread::yield(); continue; }
            back->gotLevel = true;

            GameRenderer gr = mc.GetGameRenderer();
            if (gr.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); publishDiag(); std::this_thread::yield(); continue; }
            back->gotGameRenderer = true;

            Camera cam = gr.getMainCamera();
            if (cam.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); publishDiag(); std::this_thread::yield(); continue; }
            back->gotCamera = true;

            DeltaTracker dt = mc.GetDeltaTracker();
            if (dt.GetInstance() != nullptr)
                back->partialTick = dt.getPartialTick(true);
            else
                back->partialTick = 1.0f;

            Vec3 camPos = cam.getPosition();
            back->cam.x    = camPos.getX();
            back->cam.y    = camPos.getY();
            back->cam.z    = camPos.getZ();
            back->cam.yRot = cam.getYRot();
            back->cam.xRot = cam.getXRot();
            back->cam.fov  = gr.getFov(cam, back->partialTick, true);
            if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); back->cam.fov = 70.0f; }

            auto players = level.players();
            back->rawPlayerCount = (int)players.size();
            jobject localInst = localPlayer.GetInstance();
            const double maxDistSq =
                (double)g_settings.maxDistance * (double)g_settings.maxDistance;

            back->targets.reserve(players.size());

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

                t.nameChunks = p.getFormattedNameChunks();

                {
                    Component bare = p.getName();
                    if (bare.GetInstance() != nullptr) {
                        t.playerName = bare.getString();
                        for (char& c : t.playerName)
                            c = (char)std::tolower((unsigned char)c);
                    }
                }

                t.health    = p.getHealth();
                t.maxHealth = p.getMaxHealth();

                if (!t.playerName.empty()) {
                    std::lock_guard<std::mutex> lk(g_settings.friendsMutex);
                    for (const auto& f : g_settings.friends) {
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

            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

            back->valid = true;

            lc->env->PopLocalFrame(nullptr);

            Publish(back);

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        AC_LOG("esp: loop exit; detaching");
        lc->vm->DetachCurrentThread();
        return 0;
    }
}
