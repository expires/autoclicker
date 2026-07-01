#include "SprintResetModule.h"
#include <Windows.h>
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

        if (g_settings.sprintResetMode == 1) {
            sendKey(0x53, true);
        } else {
            if (GetAsyncKeyState(0x57) & 0x8000) {
                sendKey(0x57, false);
                s_wWasHeld = true;
            }
        }
    }

    void PostClick() {
        if (!g_settings.sprintResetEnabled) return;

        if (g_settings.sprintResetMode == 1) {
            sendKey(0x53, false);
        } else {
            if (!s_wWasHeld) return;
            s_wWasHeld = false;
            sendKey(0x57, true);
        }
    }
}
