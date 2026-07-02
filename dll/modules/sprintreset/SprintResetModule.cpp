#include "SprintResetModule.h"
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <random>
#include <thread>
#include "../../config/Settings.h"
#include "../../teardown/Teardown.h"

namespace SprintResetModule {
    static std::atomic<bool> s_inProgress{false};
    static std::atomic<bool> s_physicalW{false};
    static bool              s_pending = false;

    static void sendKey(DWORD vk, bool down) {
        INPUT in         = {};
        in.type          = INPUT_KEYBOARD;
        in.ki.wScan      = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
        in.ki.dwFlags    = KEYEVENTF_SCANCODE | (down ? 0u : KEYEVENTF_KEYUP);
        in.ki.dwExtraInfo = kInjectedExtraInfo;
        SendInput(1, &in, sizeof(INPUT));
    }

    void NotePhysicalKey(WPARAM vk, bool down) {
        if (vk == 0x57)
            s_physicalW.store(down, std::memory_order_relaxed);
    }

    void PreClick(bool entityHit) {
        s_pending = g_settings.sprintResetEnabled && entityHit;
    }

    void PostClick() {
        if (!s_pending) return;
        s_pending = false;

        if (s_inProgress.exchange(true)) return;

        const int delay    = g_settings.sprintResetDelay;
        const int duration = g_settings.sprintResetDuration;
        const int mode     = g_settings.sprintResetMode;

        Teardown::BeginBackgroundTask();
        std::thread([delay, duration, mode]() {
            Teardown::BackgroundTaskGuard guard;
            if (delay > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));

            const bool wHeld = (GetAsyncKeyState(0x57) & 0x8000) != 0;
            s_physicalW.store(wHeld, std::memory_order_relaxed);

            if (wHeld)     sendKey(0x57, false);
            if (mode == 1) sendKey(0x53, true);

            if (duration > 0) {
                static thread_local std::mt19937 rng(std::random_device{}());
                std::uniform_int_distribution<int> jitter(
                    (int)(duration * 0.9f), (int)(duration * 1.1f));
                std::this_thread::sleep_for(std::chrono::milliseconds(jitter(rng)));
            }

            if (mode == 1) sendKey(0x53, false);
            if (wHeld && s_physicalW.load(std::memory_order_relaxed))
                sendKey(0x57, true);

            s_inProgress = false;
        }).detach();
    }
}
