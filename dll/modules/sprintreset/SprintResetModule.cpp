#include "SprintResetModule.h"
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <random>
#include <thread>
#include "../../config/Settings.h"

namespace SprintResetModule {
    static std::atomic<bool> s_inProgress{false};
    static bool              s_wWasHeld   = false;
    static bool              s_pending    = false;

    static void sendKey(DWORD vk, bool down) {
        INPUT in      = {};
        in.type       = INPUT_KEYBOARD;
        in.ki.wScan   = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
        in.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0u : KEYEVENTF_KEYUP);
        SendInput(1, &in, sizeof(INPUT));
    }

    void PreClick(bool entityHit) {
        if (!g_settings.sprintResetEnabled || !entityHit) {
            s_pending = false;
            return;
        }
        s_wWasHeld = (GetAsyncKeyState(0x57) & 0x8000) != 0;
        s_pending  = true;
    }

    void PostClick() {
        if (!s_pending) return;
        s_pending = false;

        if (s_inProgress.exchange(true)) return;

        const int  delay    = g_settings.sprintResetDelay;
        const int  duration = g_settings.sprintResetDuration;
        const int  mode     = g_settings.sprintResetMode;
        const bool wHeld    = s_wWasHeld;

        std::thread([delay, duration, mode, wHeld]() {
            if (delay > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));

            if (wHeld)     sendKey(0x57, false);
            if (mode == 1) sendKey(0x53, true);

            if (duration > 0) {
                static thread_local std::mt19937 rng(std::random_device{}());
                std::uniform_int_distribution<int> jitter(
                    (int)(duration * 0.9f), (int)(duration * 1.1f));
                std::this_thread::sleep_for(std::chrono::milliseconds(jitter(rng)));
            }

            if (mode == 1) sendKey(0x53, false);
            if (wHeld)     sendKey(0x57, true);

            s_inProgress = false;
        }).detach();
    }
}
