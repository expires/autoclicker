#include <Windows.h>
#include <thread>

#include "Settings.h"
#include "Teardown.h"
#include "modules/autoclicker/AutoclickerModule.h"
#include "modules/esp/EspModule.h"
#include "modules/macros/MacrosModule.h"
#include "modules/aim/AimAssistModule.h"
#include "modules/leap/LeapModule.h"
#include "modules/autoability/AutoAbilityModule.h"
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
        // Register every worker handle with Teardown so the unload path
        // can WaitForMultipleObjects on them before unmapping the DLL.
        // Without this, uninjecting while aim assist / jitter is mid
        // SendInput crashes MC: those threads are still executing inside
        // the DLL when FreeLibraryAndExitThread unmaps the code pages.
        Teardown::RegisterWorker(CreateThread(nullptr, 0, AutoclickerModule::init, instance, 0, nullptr));
        Teardown::RegisterWorker(CreateThread(nullptr, 0, EspModule::init,         instance, 0, nullptr));
        Teardown::RegisterWorker(CreateThread(nullptr, 0, MacrosModule::init,      instance, 0, nullptr));
        Teardown::RegisterWorker(CreateThread(nullptr, 0, AimAssistModule::init,   instance, 0, nullptr));
        Teardown::RegisterWorker(CreateThread(nullptr, 0, LeapModule::init,        instance, 0, nullptr));
        Teardown::RegisterWorker(CreateThread(nullptr, 0, AutoAbilityModule::init, instance, 0, nullptr));
        Teardown::RegisterWorker(CreateThread(nullptr, 0, FriendsModule::init,     instance, 0, nullptr));
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        // Flush before teardown — covers paths that skip the menu-close
        // save (UNLOAD button, game closed with overlay still open, etc.).
        g_settings.Save();
        Overlay::Shutdown();
    }
    return TRUE;
}
