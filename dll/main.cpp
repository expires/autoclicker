#include <Windows.h>
#include <thread>

#include "modules/autoclicker/AutoclickerModule.h"

using namespace std::chrono;

BOOL APIENTRY DllMain(const HINSTANCE instance, const DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
        CreateThread(nullptr, 0, AutoclickerModule::init, instance, 0, nullptr);
    }
    return TRUE;
}