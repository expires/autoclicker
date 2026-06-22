#include "FriendsModule.h"
#include "../../config/Settings.h"
#include "../../SDK/Minecraft.h"
#include "Mappings.h"
#include "Platform.h"
#include "../autoclicker/AutoclickerModule.h"
#include "../../overlay/Overlay.h"
#include "../../overlay/Notifications.h"
#include "../../logger/Logger.h"
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

                    static jclass apClass = nullptr;
                    JClass(apClass, MC_AbstractClientPlayer);
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

    static int ToggleFriend(const std::string& name)
    {
        if (name.empty()) return 0;

        std::lock_guard<std::mutex> lk(g_settings.friendsMutex);
        for (auto it = g_settings.friends.begin(); it != g_settings.friends.end(); ++it) {
            if (*it == name) {
                g_settings.friends.erase(it);
                return -1;
            }
        }
        g_settings.friends.push_back(name);
        return 1;
    }

    DWORD WINAPI init(LPVOID )
    {
        LOG("friends: thread start");
        while (!AutoclickerModule::destruct && !jvmReady())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (AutoclickerModule::destruct) return 0;

        if (lc->vm->AttachCurrentThread(reinterpret_cast<void**>(&lc->env), nullptr) != JNI_OK)
            return 0;
        if (lc->env == nullptr) return 0;
        LOG("friends: attached; entering loop");

        Minecraft  mc;
        const HWND mcWindow = FindGameWindow();
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
                    const int r = ToggleFriend(name);
                    if (r != 0) {
                        g_settings.Save();
                        if (r > 0)
                            Notifications::Push("Added " + name + " as friend",
                                                Notifications::Kind::Enabled);
                        else
                            Notifications::Push("Removed " + name + " from friends",
                                                Notifications::Kind::Disabled);
                    }
                }
            }

            prevDown = downNow;
        }

        LOG("friends: loop exit; detaching");
        lc->vm->DetachCurrentThread();
        return 0;
    }
}
