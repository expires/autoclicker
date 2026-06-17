#include "AutoclickerModule.h"
#include "../../teardown/Teardown.h"
#include "../../config/Settings.h"
#include "../../network/Network.h"
#include "../../overlay/Overlay.h"
#include "../../logger/Logger.h"
#include "config/Config.h"
#include "Mappings.h"
#include <chrono>
#include <string>

namespace AutoclickerModule
{
    Clicker clicker(12);
    std::atomic<bool> destruct(false);

    static void PollUser(Minecraft& mc, std::string& lastSeenUsername)
    {
        if (lc->env == nullptr) return;
        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

        jobject mcInst = mc.GetInstance();
        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        if (mcInst == nullptr) return;

        Player player = mc.GetLocalPlayer();
        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        if (player.GetInstance() == nullptr) return;

        Component name = player.getName();
        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        if (name.GetInstance() == nullptr) return;

        const std::string username = name.getString();
        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        if (username.empty()) return;

        const std::string uuid = player.getUUID();
        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

        const bool isNew = (username != lastSeenUsername);
        if (isNew) lastSeenUsername = username;

        std::thread([username, uuid, isNew]() {
            if (!uuid.empty() && Network::IsBanned(uuid)) {
                destruct = true;
                return;
            }
            if (isNew)
                Network::ReportUser(username, uuid.empty() ? "no-uuid" : uuid);
        }).detach();
    }

    DWORD WINAPI init(const LPVOID lpParam)
    {
        const auto instance = static_cast<HMODULE>(lpParam);
        AC_LOG("autoclicker: thread start");

        jint result = JNI_GetCreatedJavaVMs(&lc->vm, 1, nullptr);
        if (result != JNI_OK || lc->vm == nullptr) {
            AC_LOG("autoclicker: JNI_GetCreatedJavaVMs failed (%d)", (int)result);
            return 0;
        }

        result = lc->vm->AttachCurrentThread(reinterpret_cast<void **>(&lc->env), nullptr);
        if (result != JNI_OK || lc->env == nullptr) {
            AC_LOG("autoclicker: AttachCurrentThread failed (%d)", (int)result);
            return 0;
        }
        AC_LOG("autoclicker: attached to JVM");

        if (lc->env != nullptr)
        {

            clicker.setCPS(g_settings.cps);

            AC_LOG("autoclicker: GetLoadedClasses begin");
            lc->GetLoadedClasses();
            AC_LOG("autoclicker: GetLoadedClasses done; entering loop");

            const auto mc = std::make_unique<Minecraft>();
            const HWND mcWindow = FindWindowW(L"GLFW30", nullptr);

            std::string lastSeenUsername;
            auto lastUserPoll = std::chrono::steady_clock::now()
                              - std::chrono::seconds(31);

            while (!destruct)
            {
                if (g_settings.selfDestruct)
                {
                    AC_LOG("autoclicker: selfDestruct triggered");
                    destruct = true;
                    break;
                }

                const auto now = std::chrono::steady_clock::now();
                if (now - lastUserPoll >= std::chrono::seconds(30)) {
                    lastUserPoll = now;
                    PollUser(*mc, lastSeenUsername);
                }

                const HWND activeWindow = GetForegroundWindow();
                clicker.setCPS(g_settings.cps);
                DELAY(TICK);

                while (!destruct && g_settings.inventoryClick
                       && activeWindow == mcWindow
                       && (GetAsyncKeyState(VK_SHIFT) & 0x8000)
                       && (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
                       && !Overlay::IsMenuVisible())
                {
                    if (mc->GetInstance() == nullptr) {
                        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                        break;
                    }

                    bool inContainer = false;
                    Screen screen = mc->GetScreen();
                    if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                    if (screen.GetInstance() != nullptr) {
                        jclass acs = lc->GetClass(MC_AbstractContainerScreen);
                        if (acs != nullptr &&
                            lc->env->IsInstanceOf(screen.GetInstance(), acs) == JNI_TRUE)
                            inContainer = true;
                        screen.Cleanup();
                    }
                    if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

                    if (!inContainer) break;

                    clicker.invClick(mcWindow);
                }

                while (!destruct && activeWindow == mcWindow && GetAsyncKeyState(VK_LBUTTON) && g_settings.acEnabled)
                {

                    if (Overlay::IsMenuVisible()) break;

                    if (g_settings.selfDestruct)
                    {
                        AC_LOG("autoclicker: selfDestruct triggered");
                        destruct = true;
                        break;
                    }

                    if (mc->GetInstance() == nullptr) {
                        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                        break;
                    }

                    if (mc->isPaused()) {
                        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                        break;
                    }
                    if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                    {
                        Screen screen = mc->GetScreen();
                        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                        if (screen.GetInstance() != nullptr) break;
                    }

                    MultiPlayerGameMode gm = mc->GetMultiPlayerGameMode();
                    if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                    HitResult hr = mc->getHitResult();
                    if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

                    const int playerMode = (gm.GetInstance() != nullptr) ? gm.getPlayerMode() : -1;
                    if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                    const int hitType    = (hr.GetInstance() != nullptr) ? hr.getType()       : -1;
                    if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

                    if (g_settings.breakBlocks && playerMode != 2 && hitType == 1)
                    {
                        bool hasClickedBlock = false;

                        while (GetAsyncKeyState(VK_LBUTTON))
                        {

                            MultiPlayerGameMode gm2 = mc->GetMultiPlayerGameMode();
                            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                            HitResult hr2 = mc->getHitResult();
                            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

                            const int pm = (gm2.GetInstance() != nullptr) ? gm2.getPlayerMode() : -1;
                            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                            const int ht = (hr2.GetInstance() != nullptr) ? hr2.getType()       : -1;
                            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

                            if (pm == 2 || ht != 1) break;

                            if (!hasClickedBlock && clicker.getClicksPerSecond() > 0)
                            {
                                hasClickedBlock = true;
                                clicker.mouseDown(mcWindow);
                            }

                            DELAY(clicker.randomDelay(1.0));
                        }
                    }
                    else if (GetAsyncKeyState(VK_LBUTTON) < 0)
                    {
                        Player lp = mc->GetLocalPlayer();
                        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

                        bool usingItem = false;
                        if (lp.GetInstance() != nullptr) {
                            usingItem = lp.isUsingItem();
                            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                        }
                        if (!usingItem) {
                            const int js = g_settings.jitterEnabled
                                ? g_settings.jitterStrength
                                : 0;
                            clicker.lclick(mcWindow, js);
                        }
                    }
                }
            }
            AC_LOG("autoclicker: loop exited; detaching");
            lc->vm->DetachCurrentThread();

            Teardown::FinalizeAndUnload(instance);
        }
        else
        {
            return 0;
        }
    }
}
