#include "SprintResetModule.h"
#include <Windows.h>
#include <thread>
#include <chrono>
#include "../../config/Settings.h"

namespace SprintResetModule {
    static bool s_wWasHeld = false;

    static void sendKey(DWORD vk, bool down) {
        INPUT in      = {};
        in.type       = INPUT_KEYBOARD;
        in.ki.wScan   = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
        in.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0u : KEYEVENTF_KEYUP);
        SendInput(1, &in, sizeof(INPUT));
    }

    void PreClick(bool entityHit) {
        if (!g_settings.sprintResetEnabled || !entityHit) return;

        if (g_settings.sprintResetDelay > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(g_settings.sprintResetDelay));

        if (GetAsyncKeyState(0x57) & 0x8000) {
            sendKey(0x57, false);
            s_wWasHeld = true;
        }
        if (g_settings.sprintResetMode == 1)
            sendKey(0x53, true);
    }

    void PostClick() {
        if (!g_settings.sprintResetEnabled) return;

        if (g_settings.sprintResetDuration > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(g_settings.sprintResetDuration));

        if (g_settings.sprintResetMode == 1)
            sendKey(0x53, false);
        if (s_wWasHeld) {
            s_wWasHeld = false;
            sendKey(0x57, true);
        }
    }
}
