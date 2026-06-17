#include "FriendsModule.h"
#include "../../Settings.h"
#include "../../SDK/Minecraft.h"
#include "Mappings.h"
#include "../autoclicker/AutoclickerModule.h"
#include "../../overlay/Overlay.h"
#include "../../Logger.h"
#include <cctype>
#include <chrono>
#include <string>
#include <thread>

namespace FriendsModule
{
    static bool jvmReady()
    {
        return lc->vm != nullptr && lc->classesLoaded.load(std::memory_order_acquire);
    }

    static std::string HoveredPlayerName(Minecraft& mc)
    {
        std::string out;

        if (lc->env->PushLocalFrame(16) != 0) {
            lc->env->ExceptionClear();
            return out;
        }

        HitResult hr = mc.getHitResult();
        if (hr.GetInstance() != nullptr && hr.getType() == 2) {
            EntityHitResult ehr = hr.getEntityHitResult();
            if (ehr.GetInstance() != nullptr) {
                Entity ent = ehr.getEntity();
                if (ent.GetInstance() != nullptr) {

                    jclass apClass = lc->GetClass(MC_AbstractClientPlayer);
                    if (apClass != nullptr &&
                        lc->env->IsInstanceOf(ent.GetInstance(), apClass) == JNI_TRUE)
                    {
                        Component bare = ent.getName();
                        if (bare.GetInstance() != nullptr) {
                            out = bare.getString();
                            for (char& c : out)
                                c = (char)std::tolower((unsigned char)c);
                        }
                    }
                }
            }
        }

        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        lc->env->PopLocalFrame(nullptr);
        return out;
    }

    static bool ToggleFriend(const std::string& name)
    {
        if (name.empty()) return false;

        bool changed = false;
        {
            std::lock_guard<std::mutex> lk(g_settings.friendsMutex);
            for (auto it = g_settings.friends.begin(); it != g_settings.friends.end(); ++it) {
                if (*it == name) {
                    g_settings.friends.erase(it);
                    changed = true;
                    break;
                }
            }
            if (!changed) {
                g_settings.friends.push_back(name);
                changed = true;
            }
        }
        return changed;
    }

    DWORD WINAPI init(LPVOID )
    {
        AC_LOG("friends: thread start");
        while (!AutoclickerModule::destruct && !jvmReady())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (AutoclickerModule::destruct) return 0;

        if (lc->vm->AttachCurrentThread(reinterpret_cast<void**>(&lc->env), nullptr) != JNI_OK)
            return 0;
        if (lc->env == nullptr) return 0;
        AC_LOG("friends: attached; entering loop");

        Minecraft  mc;
        const HWND mcWindow = FindWindowW(L"GLFW30", nullptr);
        if (mcWindow == nullptr) {
            lc->vm->DetachCurrentThread();
            return 0;
        }

        bool prevDown = false;

        while (!AutoclickerModule::destruct)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            const int key = g_settings.friendKey;

            if (key <= 0 || key > 0xFE) { prevDown = false; continue; }

            if (GetForegroundWindow() != mcWindow) { prevDown = false; continue; }
            if (Overlay::IsMenuVisible())          { prevDown = false; continue; }

            const bool downNow = (GetAsyncKeyState(key) & 0x8000) != 0;

            if (downNow && !prevDown) {
                const std::string name = HoveredPlayerName(mc);
                if (!name.empty()) {
                    if (ToggleFriend(name)) g_settings.Save();
                }
            }

            prevDown = downNow;
        }

        AC_LOG("friends: loop exit; detaching");
        lc->vm->DetachCurrentThread();
        return 0;
    }
}
