#pragma once
#include <Windows.h>

namespace Teardown
{

    void RegisterWorker(HANDLE thread);

    [[noreturn]] void FinalizeAndUnload(HMODULE instance);
}
