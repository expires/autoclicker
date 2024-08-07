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

Settings currentSettings = {true,8,true,false};
Clicker clicker;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_COPYDATA)
    {
        PCOPYDATASTRUCT pcds = (PCOPYDATASTRUCT)lParam;
        if (pcds->dwData == 1) {
            Settings* newSettings = (Settings*)pcds->lpData;
            currentSettings = *newSettings;
            clicker.setCps(currentSettings.cps);
            return TRUE;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void static init(HMODULE instance)
{
    jint result = JNI_GetCreatedJavaVMs(&lc->vm, 1, nullptr);
    if (result != JNI_OK || lc->vm == nullptr) return;

    result = lc->vm->AttachCurrentThread((void**)&lc->env, nullptr);
    if (result != JNI_OK || lc->env == nullptr) return;
    if (lc->env != nullptr)
    {
        lc->GetLoadedClasses();
        const auto mc = std::make_unique<Minecraft>();
        const HWND mcWindow = FindWindowW(L"GLFW30", nullptr);

        WNDCLASS wc = { 0 };
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = "SettingsReceiverClass";

        if (!RegisterClass(&wc)) return;

        HWND hwndReceiver = CreateWindowEx(0, "SettingsReceiverClass", "SettingsReceiver", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
        if (!hwndReceiver) return;


        while (true) {
            MSG msg;
            while (PeekMessage(&msg, hwndReceiver, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            if (currentSettings.destruct) break;

            HWND activeWindow = GetForegroundWindow();
            DELAY(50);

            while (activeWindow == mcWindow && currentSettings.active) {

                if (currentSettings.destruct) break;
                if (GetAsyncKeyState(VK_END)) currentSettings.destruct = true;
                if (currentSettings.destruct) break;
                if (mc->GetScreen().isPauseScreen()) break;
                if (mc->GetScreen().shouldCloseOnEsc()) break;

                if (currentSettings.breakBlocks && (GetAsyncKeyState(VK_LBUTTON) < 0) && mc->GetMultiPlayerGameMode().isDestroying())
                {
                    bool isFirst = true;
                    while (true) {
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
                    if (currentSettings.breakBlocks && clicker.getClicksPerSecond() == 0 && mc->GetMultiPlayerGameMode().isDestroying()) continue;
                    clicker.click(mcWindow);
                }

                DELAY(5)
            }
        }

        if (hwndReceiver)
        {
            DestroyWindow(hwndReceiver);
            hwndReceiver = nullptr;
        }
        UnregisterClass("SettingsReceiverClass", GetModuleHandle(nullptr));
    }
    FreeLibrary(instance);
}

bool __stdcall DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    static std::thread main_thread;

    if (reason == DLL_PROCESS_ATTACH)
    {
        main_thread = std::thread([instance] { init(instance); });
        if (main_thread.joinable())
            main_thread.detach();
    }
    return true;
}