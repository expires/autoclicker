#include "AutoclickerModule.h"

namespace AutoclickerModule
{
    Clicker clicker(CPS);
    std::atomic<bool> destruct(false);

    DWORD WINAPI init(const LPVOID lpParam)
    {
        const auto instance = static_cast<HMODULE>(lpParam);

        jint result = JNI_GetCreatedJavaVMs(&lc->vm, 1, nullptr);
        if (result != JNI_OK || lc->vm == nullptr)
        {
            printf("[AC] Failed to get JVM (result=%d)\n", result);
            return 0;
        }
        printf("[AC] Got JVM\n");

        result = lc->vm->AttachCurrentThread(reinterpret_cast<void **>(&lc->env), nullptr);
        if (result != JNI_OK || lc->env == nullptr)
        {
            printf("[AC] Failed to attach thread (result=%d)\n", result);
            return 0;
        }
        printf("[AC] Attached thread\n");

        if (lc->env != nullptr)
        {
            lc->GetLoadedClasses();
            printf("[AC] Loaded classes\n");

            jclass mcClass = lc->GetClass("net.minecraft.client.Minecraft");
            printf("[AC] Minecraft class: %s\n", mcClass ? "OK" : "NULL");

            jclass playerClass = lc->GetClass("net.minecraft.client.player.LocalPlayer");
            printf("[AC] LocalPlayer class: %s\n", playerClass ? "OK" : "NULL");

            jclass screenClass = lc->GetClass("net.minecraft.client.gui.screens.Screen");
            printf("[AC] Screen class: %s\n", screenClass ? "OK" : "NULL");

            jclass hitResultClass = lc->GetClass("net.minecraft.world.phys.HitResult");
            printf("[AC] HitResult class: %s\n", hitResultClass ? "OK" : "NULL");

            jclass gameModeClass = lc->GetClass("net.minecraft.client.multiplayer.MultiPlayerGameMode");
            printf("[AC] MultiPlayerGameMode class: %s\n", gameModeClass ? "OK" : "NULL");

            const auto mc = std::make_unique<Minecraft>();
            const HWND mcWindow = FindWindowW(L"GLFW30", nullptr);
            printf("[AC] GLFW30 window: %s\n", mcWindow ? "OK" : "NULL");
            while (!destruct)
            {
                const HWND activeWindow = GetForegroundWindow();
                DELAY(TICK);

                while (activeWindow == mcWindow && GetAsyncKeyState(VK_LBUTTON))
                {
                    if (GetAsyncKeyState(VK_END))
                    {
                        destruct = true;
                        break;
                    }

                    if (mc->GetScreen().isPauseScreen() || mc->GetScreen().shouldCloseOnEsc())
                        break;

                    if (mc->GetMultiPlayerGameMode().getPlayerMode() != 2 && mc->getHitResult().getType() == 1)
                    {
                        bool hasClickedBlock = false;

                        while (GetAsyncKeyState(VK_LBUTTON) && mc->GetMultiPlayerGameMode().getPlayerMode() != 2 && mc->getHitResult().getType() == 1)
                        {
                            if (!hasClickedBlock && clicker.getClicksPerSecond() > 0)
                            {
                                hasClickedBlock = true;
                                clicker.mouseDown(mcWindow);
                            }

                            DELAY(clicker.randomDelay(1000));
                        }
                    }
                    else if (GetAsyncKeyState(VK_LBUTTON) < 0 && !mc->GetLocalPlayer().isUsingItem())
                    {
                        clicker.lclick(mcWindow);
                    }
                }
            }
            lc->vm->DetachCurrentThread();
            FreeLibraryAndExitThread(instance, 0);
            return 0;
        }
        else
        {
            return 0;
        }
    }
}