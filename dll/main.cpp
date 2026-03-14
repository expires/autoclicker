#include <Windows.h>
#include <thread>

#include "modules/autoclicker/AutoclickerModule.h"
#include "modules/llm/LLMModule.h"
#include "overlay/Overlay.h"

using namespace std::chrono;

BOOL APIENTRY DllMain(const HINSTANCE instance, const DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
        OutputDebugStringA("[MCBot] DLL loaded\n");
        Overlay::Init();
        CreateThread(nullptr, 0, AutoclickerModule::init, instance, 0, nullptr);
        CreateThread(nullptr, 0, LLMModule::init, nullptr, 0, nullptr);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        Overlay::Shutdown();
    }
    return TRUE;
}
