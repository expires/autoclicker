#include <Windows.h>
#include <chrono>
#include <thread>
#include "SDK/Minecraft.h"
#include "Clicker/Clicker.h"
#include <iostream>
#include <cmath>

using namespace std::chrono;



constexpr double CLICKS_PER_SECOND = 12.1;
constexpr int TICK_SPEED = 50;
Clicker clicker(CLICKS_PER_SECOND);

namespace
{

    std::atomic<bool> destruct = false;

    DWORD WINAPI initialize(const LPVOID lpParam)
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
                DELAY(TICK_SPEED);

                auto lastCheckTime = std::chrono::steady_clock::now();
                int delay = static_cast<int>(std::ceil(((1000.0 / (CLICKS_PER_SECOND + (CLICKS_PER_SECOND / 2) )))));
                const auto throttleInterval = std::chrono::milliseconds(delay);

                while (activeWindow == mcWindow && GetAsyncKeyState(VK_LBUTTON))
                {
                    if (GetAsyncKeyState(VK_END))
                    {
                        destruct = true;
                        break;
                    }
                    if (mc->GetScreen().isPauseScreen())
                        break;
                    if (!GetAsyncKeyState(VK_LSHIFT) && mc->GetScreen().shouldCloseOnEsc())
                        break;

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

                            DELAY(throttleInterval);
                        }
                        break;
                    }
                    else
                    {
                        if ((GetAsyncKeyState(VK_LBUTTON) < 0) && GetAsyncKeyState(VK_RBUTTON) >= 0)
                        {
                            int fDelay = static_cast<int>(std::ceil(((delay / 2))));
                            clicker.click(mcWindow, fDelay);
                            DELAY(clicker.randomDelay(delay))
                        }
                    }
                   
                }
            }
        }
        FreeLibraryAndExitThread(instance, 0);
        return 0;
    }
}

static BOOL APIENTRY DllMain(const HINSTANCE instance, const DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
        CreateThread(nullptr, 0, initialize, instance, 0, nullptr);
    }
    return TRUE;
}