#include "Teardown.h"
#include "overlay/Overlay.h"
#include "../config/Settings.h"
#include "../logger/Logger.h"
#include <atomic>
#include <mutex>

namespace Teardown
{

    constexpr int    kMaxWorkers              = 16;
    static std::mutex g_mu;
    static HANDLE     g_workers[kMaxWorkers]  = {};
    static int        g_workerCount           = 0;
    static std::atomic<int> g_backgroundTasks{0};

    void BeginBackgroundTask()
    {
        g_backgroundTasks.fetch_add(1, std::memory_order_acq_rel);
    }

    void EndBackgroundTask()
    {
        g_backgroundTasks.fetch_sub(1, std::memory_order_acq_rel);
    }

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
        LOG("teardown: FinalizeAndUnload begin");
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
            LOG("teardown: waiting for %d worker(s)", waitCount);
            const DWORD r = WaitForMultipleObjects((DWORD)waitCount, toWait, TRUE, 2000);
            LOG("teardown: workers joined (wait result=0x%08lX)", r);
        }

        {
            const ULONGLONG deadline = GetTickCount64() + 6000;
            int pending = g_backgroundTasks.load(std::memory_order_acquire);
            if (pending > 0)
                LOG("teardown: draining %d background task(s)", pending);
            while (pending > 0 && GetTickCount64() < deadline) {
                Sleep(10);
                pending = g_backgroundTasks.load(std::memory_order_acquire);
            }
            if (pending > 0)
                LOG("teardown: %d background task(s) still pending at deadline", pending);
        }

        {
            std::lock_guard<std::mutex> lk(g_mu);
            for (int i = 0; i < g_workerCount; ++i) {
                if (g_workers[i]) CloseHandle(g_workers[i]);
                g_workers[i] = nullptr;
            }
            g_workerCount = 0;
        }

        g_settings.Save();

        LOG("teardown: overlay shutdown");
        Overlay::Shutdown();
        LOG("teardown: overlay shutdown complete");

        Sleep(150);

        LOG("teardown: FreeLibraryAndExitThread");
        Logger::Shutdown();
        FreeLibraryAndExitThread(instance, 0);
    }
}
