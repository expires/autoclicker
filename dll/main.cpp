#include <Windows.h>
#include <thread>

#include "config/Settings.h"
#include "teardown/Teardown.h"
#include "logger/Logger.h"
#include "modules/autoclicker/AutoclickerModule.h"
#include "modules/esp/EspModule.h"
#include "modules/macros/MacrosModule.h"
#include "modules/aim/AimAssistModule.h"
#include "modules/autoblock/AutoblockModule.h"
#include "modules/friends/FriendsModule.h"
#include "overlay/Overlay.h"

using namespace std::chrono;

static HINSTANCE g_instance = nullptr;

static DWORD WINAPI Bootstrap(LPVOID)
{
    Logger::Init();
    LOG("bootstrap: start");

    g_settings.Load();
    LOG("bootstrap: settings loaded");

    Overlay::Init();
    LOG("bootstrap: overlay hooks installed");

    Teardown::RegisterWorker(CreateThread(nullptr, 0, AutoclickerModule::init, g_instance, 0, nullptr));
    Teardown::RegisterWorker(CreateThread(nullptr, 0, EspModule::init,         g_instance, 0, nullptr));
    Teardown::RegisterWorker(CreateThread(nullptr, 0, MacrosModule::init,      g_instance, 0, nullptr));
    Teardown::RegisterWorker(CreateThread(nullptr, 0, AimAssistModule::init,   g_instance, 0, nullptr));
    Teardown::RegisterWorker(CreateThread(nullptr, 0, AutoblockModule::init,   g_instance, 0, nullptr));
    Teardown::RegisterWorker(CreateThread(nullptr, 0, FriendsModule::init,     g_instance, 0, nullptr));

    LOG("bootstrap: all worker threads spawned");
    return 0;
}

BOOL APIENTRY DllMain(const HINSTANCE instance, const DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
        g_instance = instance;
        HANDLE h = CreateThread(nullptr, 0, Bootstrap, nullptr, 0, nullptr);
        if (h) CloseHandle(h);
    }
    return TRUE;
}
