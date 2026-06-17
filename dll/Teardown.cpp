#include "Teardown.h"
#include "overlay/Overlay.h"
#include "Logger.h"
#include <mutex>

namespace Teardown
{

    constexpr int    kMaxWorkers              = 16;
    static std::mutex g_mu;
    static HANDLE     g_workers[kMaxWorkers]  = {};
    static int        g_workerCount           = 0;

    void RegisterWorker(HANDLE thread)
    {
        if (thread == nullptr) return;
        std::lock_guard<std::mutex> lk(g_mu);
        if (g_workerCount >= kMaxWorkers) {

            CloseHandle(thread);
            return;
        }
        g_workers[g_workerCount++] = thread;
    }

    [[noreturn]] void FinalizeAndUnload(HMODULE instance)
    {
        AC_LOG("teardown: FinalizeAndUnload begin");
        Overlay::BeginTeardown();

        HANDLE toWait[kMaxWorkers];
        int    waitCount = 0;
        const HANDLE self = GetCurrentThread();

        HANDLE selfReal = nullptr;
        DuplicateHandle(GetCurrentProcess(), self,
                        GetCurrentProcess(), &selfReal,
                        0, FALSE, DUPLICATE_SAME_ACCESS);

        {
            std::lock_guard<std::mutex> lk(g_mu);
            for (int i = 0; i < g_workerCount; ++i) {
                HANDLE h = g_workers[i];
                if (h == nullptr) continue;

                if (selfReal && GetThreadId(h) == GetThreadId(selfReal))
                    continue;
                toWait[waitCount++] = h;
            }
        }
        if (selfReal) CloseHandle(selfReal);

        if (waitCount > 0) {
            AC_LOG("teardown: waiting for %d worker(s)", waitCount);
            const DWORD r = WaitForMultipleObjects((DWORD)waitCount, toWait, TRUE, 2000);
            AC_LOG("teardown: workers joined (wait result=0x%08lX)", r);
        }

        {
            std::lock_guard<std::mutex> lk(g_mu);
            for (int i = 0; i < g_workerCount; ++i) {
                if (g_workers[i]) CloseHandle(g_workers[i]);
                g_workers[i] = nullptr;
            }
            g_workerCount = 0;
        }

        AC_LOG("teardown: overlay shutdown");
        Overlay::Shutdown();
        AC_LOG("teardown: overlay shutdown complete");

        Sleep(150);

        AC_LOG("teardown: FreeLibraryAndExitThread");
        FreeLibraryAndExitThread(instance, 0);
    }
}
