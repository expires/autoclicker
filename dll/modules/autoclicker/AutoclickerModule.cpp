#include "AutoclickerModule.h"


namespace AutoclickerModule
{
    Clicker clicker(15);
    std::atomic<bool> destruct(false);

    DWORD WINAPI init(const LPVOID lpParam)
    {
        const auto instance = static_cast<HMODULE>(lpParam);

        jint result = JNI_GetCreatedJavaVMs(&lc->vm, 1, nullptr);
        if (result != JNI_OK || lc->vm == nullptr)
            return 0;

        result = lc->vm->AttachCurrentThread(reinterpret_cast<void**>(&lc->env), nullptr);
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

                auto lastCheckTime = std::chrono::steady_clock::now();
                int delay = static_cast<int>(std::ceil(((( 500.0 / CPS ) + (CPS / 2)))));

                while (activeWindow == mcWindow && GetAsyncKeyState(VK_LBUTTON))
                {
                    if (GetAsyncKeyState(VK_END))
                    {
                        destruct = true;
                        break;
                    }
                    if (mc->GetScreen().isPauseScreen() || mc->GetScreen().shouldCloseOnEsc()) break;

                    bool hasClickedBlock = false;

                    if (mc->GetMultiPlayerGameMode().getPlayerMode() != 2 && mc->getHitResult().getType() == 1)
                    {
                        while (GetAsyncKeyState(VK_LBUTTON))
                        {
                            if (mc->GetMultiPlayerGameMode().getPlayerMode() == 2)
                                break;

                            if (mc->getHitResult().getType() == 1)
                            {
                                if (!hasClickedBlock && clicker.getClicksPerSecond() > 0)
                                {
                                    hasClickedBlock = true;
                                    clicker.mouseDown(mcWindow);
                                }
                            }
                            else break;
                            DELAY(delay);
                        }
                        break;
                    }
                    else
                    {
                        if ((GetAsyncKeyState(VK_LBUTTON) < 0) && GetAsyncKeyState(VK_RBUTTON) >= 0)
                        {
                            clicker.click(mcWindow, delay);
                            DELAY(delay)
                        }
                    }

                }
            }
            FreeLibraryAndExitThread(instance, 0);
            return 0;
        }
    }
}