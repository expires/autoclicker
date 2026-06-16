#include <Windows.h>
#include <thread>

#include "Settings.h"
#include "Teardown.h"
#include "modules/autoclicker/AutoclickerModule.h"
#include "modules/esp/EspModule.h"
#include "modules/macros/MacrosModule.h"
#include "modules/aim/AimAssistModule.h"
#include "modules/autoblock/AutoblockModule.h"
#include "modules/friends/FriendsModule.h"
#include "overlay/Overlay.h"

using namespace std::chrono;

BOOL APIENTRY DllMain(const HINSTANCE instance, const DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
        g_settings.Load();
        Overlay::Init();

        Teardown::RegisterWorker(CreateThread(nullptr, 0, AutoclickerModule::init, instance, 0, nullptr));
        Teardown::RegisterWorker(CreateThread(nullptr, 0, EspModule::init,         instance, 0, nullptr));
        Teardown::RegisterWorker(CreateThread(nullptr, 0, MacrosModule::init,      instance, 0, nullptr));
        Teardown::RegisterWorker(CreateThread(nullptr, 0, AimAssistModule::init,   instance, 0, nullptr));
        Teardown::RegisterWorker(CreateThread(nullptr, 0, AutoblockModule::init, instance, 0, nullptr));
        Teardown::RegisterWorker(CreateThread(nullptr, 0, FriendsModule::init,     instance, 0, nullptr));
    }
    else if (reason == DLL_PROCESS_DETACH)
    {

        g_settings.Save();
        Overlay::Shutdown();
    }
    return TRUE;
}
