#pragma once
#include <Windows.h>

namespace Teardown
{

    void RegisterWorker(HANDLE thread);

    void BeginBackgroundTask();
    void EndBackgroundTask();

    struct BackgroundTaskGuard
    {
        BackgroundTaskGuard() = default;
        ~BackgroundTaskGuard() { EndBackgroundTask(); }
        BackgroundTaskGuard(const BackgroundTaskGuard&) = delete;
        BackgroundTaskGuard& operator=(const BackgroundTaskGuard&) = delete;
    };

    [[noreturn]] void FinalizeAndUnload(HMODULE instance);
}
