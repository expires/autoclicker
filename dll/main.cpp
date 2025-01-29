#include <Windows.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>

#include "modules/autoclicker/AutoclickerModule.h"

using namespace std::chrono;

void CreateDebugConsole()
{
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
    std::cout << "[DEBUG] Console Initialized\n";
}

static BOOL APIENTRY DllMain(const HINSTANCE instance, const DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
        CreateDebugConsole();
        CreateThread(nullptr, 0, AutoclickerModule::init, instance, 0, nullptr);
    }
    return TRUE;
}