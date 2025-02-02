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
            return 0;

        result = lc->vm->AttachCurrentThread(reinterpret_cast<void **>(&lc->env), nullptr);
        if (result != JNI_OK || lc->env == nullptr)
            return 0;

        if (lc->env != nullptr)
        {
            lc->GetLoadedClasses();
            const auto mc = std::make_unique<Minecraft>();
            const HWND mcWindow = FindWindowW(L"GLFW30", nullptr);
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

                    if (mc->GetScreen().isPauseScreen() || mc->GetScreen().shouldCloseOnEsc()) break;

                    if (mc->GetMultiPlayerGameMode().getPlayerMode() != 2 && mc->getHitResult().getType() == 1)
                    {
                        bool hasClickedBlock = false;

                        while (GetAsyncKeyState(VK_LBUTTON))
                        {
                            if (mc->GetMultiPlayerGameMode().getPlayerMode() == 2 || mc->getHitResult().getType() != 1)
                                break;

                            if (!hasClickedBlock && clicker.getClicksPerSecond() > 0)
                            {
                                hasClickedBlock = true;
                                clicker.mouseDown(mcWindow);
                            }

                            DELAY(clicker.randomDelay(1000));
                        }
                    }
                    else if (GetAsyncKeyState(VK_LBUTTON) < 0 && GetAsyncKeyState(VK_RBUTTON) >= 0)
                    {
                        clicker.lclick(mcWindow);
                    }
                }
            }
            lc->vm->DetachCurrentThread();
            FreeLibraryAndExitThread(instance, 0);
            return 0;
        }
    }
}