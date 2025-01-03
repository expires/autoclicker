#include <Windows.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>

#include "modules/autoclicker/AutoclickerModule.h"

using namespace std::chrono;

static BOOL APIENTRY DllMain(const HINSTANCE instance, const DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
        CreateThread(nullptr, 0, AutoclickerModule::init, instance, 0, nullptr);
    }
    return TRUE;
}