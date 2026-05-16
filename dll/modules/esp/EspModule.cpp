#include "EspModule.h"
#include "../../Settings.h"
#include "../../SDK/Minecraft.h"
#include "Mappings.h"
#include "../autoclicker/AutoclickerModule.h"
#include <thread>
#include <chrono>

namespace EspModule
{
    std::mutex snapMutex;
    Snapshot   snapshot;

    static bool jvmReady()
    {
        return lc->vm != nullptr && lc->classesLoaded.load(std::memory_order_acquire);
    }

    DWORD WINAPI init(LPVOID lpParam)
    {
        // Wait for the autoclicker thread to attach + populate the class map.
        while (!AutoclickerModule::destruct && !jvmReady())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (AutoclickerModule::destruct) return 0;

        // Attach this thread to the JVM. lc->env is thread_local, so this gives
        // *us* an env without disturbing the autoclicker thread's.
        if (lc->vm->AttachCurrentThread(reinterpret_cast<void**>(&lc->env), nullptr) != JNI_OK)
            return 0;
        if (lc->env == nullptr) return 0;

        Minecraft mc;
        Snapshot back;

        while (!AutoclickerModule::destruct)
        {
            if (!g_settings.espEnabled)
            {
                {
                    std::lock_guard<std::mutex> lk(snapMutex);
                    snapshot.valid = false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            // PushLocalFrame bounds the local refs we create this iteration so
            // they all die at PopLocalFrame — no slow leak across iterations.
            if (lc->env->PushLocalFrame(256) != 0)
            {
                lc->env->ExceptionClear();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            back.targets.clear();
            back.valid = false;

            jobject mcInst = mc.GetInstance();
            if (mcInst == nullptr) { lc->env->PopLocalFrame(nullptr); std::this_thread::yield(); continue; }

            Player localPlayer = mc.GetLocalPlayer();
            if (localPlayer.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); std::this_thread::yield(); continue; }

            Level level = mc.GetLevel();
            if (level.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); std::this_thread::yield(); continue; }

            GameRenderer gr = mc.GetGameRenderer();
            if (gr.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); std::this_thread::yield(); continue; }

            Camera cam = gr.getMainCamera();
            if (cam.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); std::this_thread::yield(); continue; }

            // Camera snapshot
            Vec3 camPos = cam.getPosition();
            back.cam.x    = camPos.getX();
            back.cam.y    = camPos.getY();
            back.cam.z    = camPos.getZ();
            back.cam.yRot = cam.getYRot();
            back.cam.xRot = cam.getXRot();
            back.cam.fov  = gr.getFov(cam, 1.0f, true);
            if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); back.cam.fov = 70.0f; }

            // Players
            auto players = level.players();
            jobject localInst = localPlayer.GetInstance();
            for (auto& p : players)
            {
                if (lc->env->IsSameObject(p.GetInstance(), localInst)) continue;

                Target t;
                Vec3 pos = p.getPosition();
                if (pos.GetInstance() == nullptr) continue;
                t.x = pos.getX();
                t.y = pos.getY();
                t.z = pos.getZ();

                AABB box = p.getBoundingBox();
                if (box.GetInstance() != nullptr)
                {
                    t.minX = box.minX(); t.minY = box.minY(); t.minZ = box.minZ();
                    t.maxX = box.maxX(); t.maxY = box.maxY(); t.maxZ = box.maxZ();
                }
                else
                {
                    t.minX = t.x - 0.3; t.maxX = t.x + 0.3;
                    t.minY = t.y;        t.maxY = t.y + 1.8;
                    t.minZ = t.z - 0.3; t.maxZ = t.z + 0.3;
                }

                Component nameC = p.getName();
                if (nameC.GetInstance() != nullptr)
                    t.name = nameC.getString();

                back.targets.push_back(std::move(t));
            }

            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

            back.valid = true;

            lc->env->PopLocalFrame(nullptr);

            {
                std::lock_guard<std::mutex> lk(snapMutex);
                std::swap(snapshot, back);
            }
        }

        lc->vm->DetachCurrentThread();
        return 0;
    }
}
