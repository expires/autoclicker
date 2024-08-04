#include <Windows.h>
#include <chrono>
#include <thread>
#include <iostream>
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

Settings currentSettings = { true, 8, true, false }; 
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

void static init(void* instance)
{
    jsize count;
    if (JNI_GetCreatedJavaVMs(&lc->vm, 1, &count) != JNI_OK || count == 0) return;

    jint res = lc->vm->GetEnv(reinterpret_cast<void**>(&lc->env), JNI_VERSION_1_8);
    if (res == JNI_EDETACHED) res = lc->vm->AttachCurrentThread(reinterpret_cast<void**>(&lc->env), nullptr);

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
            if (currentSettings.destruct) break;
        	HWND activeWindow = GetForegroundWindow();
            DELAY(50)

            while (activeWindow == mcWindow && currentSettings.active) {
                if (currentSettings.destruct) break;

                if (GetAsyncKeyState(VK_END))
                {
	                currentSettings.destruct = true; break;
                }

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

                        if (!mc->GetMultiPlayerGameMode().isDestroying() && !isFirst) 
                        {
                            break;
                        }

                        if (mc->GetMultiPlayerGameMode().getDestroyStage() > 8 && mc->GetMultiPlayerGameMode().getDestroyStage() < 255) 
                        {
                            DELAY(750);
                        }
                    }
                }

                if ((GetAsyncKeyState(VK_LBUTTON) < 0) && GetAsyncKeyState(VK_RBUTTON) >= 0) 
                {
                    if (currentSettings.breakBlocks && clicker.getClicksPerSecond() == 0 && mc->GetMultiPlayerGameMode().isDestroying()) continue;
                    clicker.click(mcWindow);
                }

                DELAY(5)
            }

            // Process messages for settings window
            MSG msg;
            while (PeekMessage(&msg, hwndReceiver, 0, 0, PM_REMOVE)) 
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                std::cout << "Received message: " << msg.message << std::endl;
            }
        }

        if (hwndReceiver) 
        {
            DestroyWindow(hwndReceiver);
            hwndReceiver = nullptr;
        }

        // Unregister the class
        UnregisterClass("SettingsReceiverClass", GetModuleHandle(NULL));
    }
    FreeLibraryAndExitThread(static_cast<HMODULE>(instance), 0);
}

BOOL APIENTRY DllMain(const HMODULE hModule,
    const DWORD ul_reason_for_call,
    LPVOID lpReserved) {

    DisableThreadLibraryCalls(hModule);

    switch (ul_reason_for_call)
	{
    case DLL_PROCESS_ATTACH:
        CloseHandle(CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(init), hModule, 0, nullptr));
        break;
    case DLL_PROCESS_DETACH:
        break;
    default:
        break;
    }
    return TRUE;
}
