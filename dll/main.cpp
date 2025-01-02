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

        AllocConsole();
        freopen("CONOUT$", "w", stdout);  // Redirect stdout to the console
        freopen("CONOUT$", "w", stderr);  // Redirect stderr to the console

        // Print a debug message
        printf("DLL loaded successfully.\n");


        DisableThreadLibraryCalls(instance);
        CreateThread(nullptr, 0, AutoclickerModule::init, instance, 0, nullptr);
    }
    return TRUE;
}