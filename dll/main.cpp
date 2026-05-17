#include <Windows.h>
#include <thread>

#include "Settings.h"
#include "modules/autoclicker/AutoclickerModule.h"
#include "modules/esp/EspModule.h"
#include "overlay/Overlay.h"

using namespace std::chrono;

BOOL APIENTRY DllMain(const HINSTANCE instance, const DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
        g_settings.Load();
        Overlay::Init();
        CreateThread(nullptr, 0, AutoclickerModule::init, instance, 0, nullptr);
        CreateThread(nullptr, 0, EspModule::init,         instance, 0, nullptr);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        Overlay::Shutdown();
    }
    return TRUE;
}
