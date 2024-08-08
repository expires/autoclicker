#include <Windows.h>
#include <chrono>
#include <thread>
#include "SDK/Minecraft.h"
#include "Clicker/Clicker.h"

using namespace std::chrono;

struct Settings
{
    bool active;
    int cps;
    bool breakBlocks;
    bool destruct;
};

Settings settings = { true, 8, true, false };
Clicker clicker;

namespace {
    LRESULT CALLBACK wndProc(const HWND handle, const UINT msg, const WPARAM wParam, const LPARAM lParam)
    {
        if (msg == WM_COPYDATA)
        {
            if (const auto receivedSettings = reinterpret_cast<PCOPYDATASTRUCT>(lParam); receivedSettings->dwData == 1) {
                const auto* newSettings = static_cast<Settings*>(receivedSettings->lpData);
                settings = *newSettings;
                clicker.setCps(settings.cps);
                return TRUE;
            }
        }
        return DefWindowProc(handle, msg, wParam, lParam);
    }

    DWORD WINAPI initialize(const LPVOID lpParam)
    {
        const auto instance = static_cast<HMODULE>(lpParam);

        jint result = JNI_GetCreatedJavaVMs(&lc->vm, 1, nullptr);
        if (result != JNI_OK || lc->vm == nullptr) return 0;

        result = lc->vm->AttachCurrentThread(reinterpret_cast<void**>(&lc->env), nullptr);
        if (result != JNI_OK || lc->env == nullptr) return 0;

        if (lc->env != nullptr)
        {
            lc->GetLoadedClasses();
            const auto mc = std::make_unique<Minecraft>();
            const HWND mcWindow = FindWindowW(L"GLFW30", nullptr);

            WNDCLASS wc = { 0 };
            wc.lpfnWndProc = wndProc;
            wc.hInstance = GetModuleHandle(nullptr);
            wc.lpszClassName = "SettingsReceiverClass";

            if (!RegisterClass(&wc)) return 0;

            HWND settingsReceiver = CreateWindowEx(0, "SettingsReceiverClass", "SettingsReceiver", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
            if (!settingsReceiver) return 0;

            while (true)
            {
                MSG msg;
                while (PeekMessage(&msg, settingsReceiver, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }

                if (settings.destruct) break;

                const HWND activeWindow = GetForegroundWindow();
                DELAY(50);

                while (activeWindow == mcWindow && settings.active)
                {
                    if (settings.destruct) break;
                    if (GetAsyncKeyState(VK_END)) settings.destruct = true;
                    if (settings.destruct) break;
                    if (mc->GetScreen().isPauseScreen()) break;
                    if (mc->GetScreen().shouldCloseOnEsc()) break;

                    if (settings.breakBlocks && (GetAsyncKeyState(VK_LBUTTON) < 0) && mc->GetMultiPlayerGameMode().isDestroying())
                    {
                        bool isFirst = true;
                        while (true)
                        {
                            if (isFirst)
                            {
                                if (!mc->GetMultiPlayerGameMode().isDestroying()) continue;
                                clicker.mouseDown(mcWindow);
                                isFirst = false;
                            }

                            if (!mc->GetMultiPlayerGameMode().isDestroying() && !isFirst) break;
                            if (mc->GetMultiPlayerGameMode().getDestroyStage() > 8 && mc->GetMultiPlayerGameMode().getDestroyStage() < 255) DELAY(500);
                        }
                    }

                    if ((GetAsyncKeyState(VK_LBUTTON) < 0) && GetAsyncKeyState(VK_RBUTTON) >= 0)
                    {
                        if (settings.breakBlocks && clicker.getClicksPerSecond() == 0 && mc->GetMultiPlayerGameMode().isDestroying()) continue;
                        clicker.click(mcWindow);
                    }

                    DELAY(5);
                }
            }

            if (settingsReceiver)
            {
                DestroyWindow(settingsReceiver);
                settingsReceiver = nullptr;
            }
            UnregisterClass("SettingsReceiverClass", GetModuleHandle(nullptr));
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
