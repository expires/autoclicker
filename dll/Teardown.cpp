#include "Teardown.h"
#include "overlay/Overlay.h"
#include <mutex>

namespace Teardown
{
    // Fixed-size registry. We currently spawn 7 worker threads from DllMain
    // (autoclicker, esp, macros, aim, leap, autoability, friends); the cap
    // is sized with headroom so a future module can register without a
    // capacity change. Overflow is silently dropped — better than allocating
    // during init, and a missing entry just means that worker doesn't get
    // waited on (worst case: same crash class this file is meant to prevent,
    // so the cap is generous on purpose).
    constexpr int    kMaxWorkers              = 16;
    static std::mutex g_mu;
    static HANDLE     g_workers[kMaxWorkers]  = {};
    static int        g_workerCount           = 0;

    void RegisterWorker(HANDLE thread)
    {
        if (thread == nullptr) return;
        std::lock_guard<std::mutex> lk(g_mu);
        if (g_workerCount >= kMaxWorkers) {
            // Out of slots — close the handle the caller handed us so we
            // don't leak it. The thread itself keeps running; we just lose
            // the ability to wait on it during teardown.
            CloseHandle(thread);
            return;
        }
        g_workers[g_workerCount++] = thread;
    }

    [[noreturn]] void FinalizeAndUnload(HMODULE instance)
    {
        // Snapshot under the mutex, then drop it before any blocking wait
        // so a misbehaving worker can't hold the registry hostage. We also
        // skip our own handle — WaitForSingleObject on the current thread's
        // handle would deadlock waiting on ourselves.
        HANDLE toWait[kMaxWorkers];
        int    waitCount = 0;
        const HANDLE self = GetCurrentThread();
        // Pseudo-handle (-2) returned by GetCurrentThread isn't directly
        // comparable to a real handle from DuplicateHandle / CreateThread.
        // Duplicate it into a real handle scoped to this process so the
        // self-skip check below actually matches the registered entry.
        HANDLE selfReal = nullptr;
        DuplicateHandle(GetCurrentProcess(), self,
                        GetCurrentProcess(), &selfReal,
                        0, FALSE, DUPLICATE_SAME_ACCESS);

        {
            std::lock_guard<std::mutex> lk(g_mu);
            for (int i = 0; i < g_workerCount; ++i) {
                HANDLE h = g_workers[i];
                if (h == nullptr) continue;
                // GetThreadId comparison sidesteps the pseudo-handle issue
                // entirely if DuplicateHandle ever fails — same numeric
                // thread ID means same thread regardless of handle value.
                if (selfReal && GetThreadId(h) == GetThreadId(selfReal))
                    continue;
                toWait[waitCount++] = h;
            }
        }
        if (selfReal) CloseHandle(selfReal);

        // Two-second cap. Workers poll the destruct flag on a 10-50ms cadence
        // so any well-behaved thread returns inside a few hundred ms; the cap
        // just prevents a thread genuinely wedged in a long JNI call from
        // blocking unload forever. If a worker is still running after the
        // timeout, FreeLibraryAndExitThread still happens — accepted risk
        // since the alternative is a hung MC.
        if (waitCount > 0) {
            // WaitForMultipleObjects caps at MAXIMUM_WAIT_OBJECTS (64),
            // well above our 16-slot registry, so one call covers all.
            WaitForMultipleObjects((DWORD)waitCount, toWait, TRUE, 2000);
        }

        // Close every registered handle now that we're done with them.
        // The DLL is about to be unmapped, so this is more about hygiene
        // than necessity, but skipping it would leak handles in any future
        // re-inject path that lives across multiple Init/Shutdown cycles.
        {
            std::lock_guard<std::mutex> lk(g_mu);
            for (int i = 0; i < g_workerCount; ++i) {
                if (g_workers[i]) CloseHandle(g_workers[i]);
                g_workers[i] = nullptr;
            }
            g_workerCount = 0;
        }

        // Tear the overlay down — disables MinHook hooks, restores the
        // window proc, shuts ImGui. After this returns, MC's render thread
        // will no longer enter our wglSwapBuffers hook on *new* calls.
        Overlay::Shutdown();

        // Drain in-flight render-thread calls. MH_DisableHook patches the
        // prologue back but doesn't stop a thread already executing past
        // the prologue inside our hook function. 150ms is well above one
        // frame at any sane FPS, so any call that was mid-hook at Shutdown
        // time has long since returned to MC.
        Sleep(150);

        FreeLibraryAndExitThread(instance, 0);
    }
}
