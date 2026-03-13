#include "AutoclickerModule.h"

namespace AutoclickerModule
{
    Clicker clicker(12);
    std::atomic<bool> destruct(false);

    static int loadCPS(HMODULE hModule)
    {
        char path[MAX_PATH];
        GetModuleFileNameA(hModule, path, MAX_PATH);
        char *slash = strrchr(path, '\\');
        if (slash) *slash = '\0';
        strcat_s(path, "\\ac_config.json");

        FILE *f;
        fopen_s(&f, path, "r");
        if (!f) return 12;

        char buf[256] = {};
        fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);

        // Find "CPS" key and read the integer after the colon
        const char *key = strstr(buf, "\"CPS\"");
        if (!key) return 12;
        const char *colon = strchr(key, ':');
        if (!colon) return 12;

        int cps = 12;
        sscanf_s(colon + 1, " %d", &cps);
        return max(1, min(cps, 50));
    }

    DWORD WINAPI init(const LPVOID lpParam)
    {
        const auto instance = static_cast<HMODULE>(lpParam);

        jint result = JNI_GetCreatedJavaVMs(&lc->vm, 1, nullptr);
        if (result != JNI_OK || lc->vm == nullptr)
            return 0;

        result = lc->vm->AttachCurrentThread(reinterpret_cast<void **>(&lc->env), nullptr);
        if (result != JNI_OK || lc->env == nullptr)
            return 0;

        if (lc->env != nullptr)
        {
            clicker.setCPS(loadCPS(instance));
            lc->GetLoadedClasses();

            const auto mc = std::make_unique<Minecraft>();
            const HWND mcWindow = FindWindowW(L"GLFW30", nullptr);
            while (!destruct)
            {
                const HWND activeWindow = GetForegroundWindow();
                DELAY(TICK);

                while (activeWindow == mcWindow && GetAsyncKeyState(VK_LBUTTON))
                {
                    if (GetAsyncKeyState(VK_END))
                    {
                        destruct = true;
                        break;
                    }

                    if (mc->GetScreen().isPauseScreen() || mc->GetScreen().shouldCloseOnEsc())
                        break;

                    if (mc->GetMultiPlayerGameMode().getPlayerMode() != 2 && mc->getHitResult().getType() == 1)
                    {
                        bool hasClickedBlock = false;

                        while (GetAsyncKeyState(VK_LBUTTON) && mc->GetMultiPlayerGameMode().getPlayerMode() != 2 && mc->getHitResult().getType() == 1)
                        {
                            if (!hasClickedBlock && clicker.getClicksPerSecond() > 0)
                            {
                                hasClickedBlock = true;
                                clicker.mouseDown(mcWindow);
                            }

                            DELAY(clicker.randomDelay(1.0));
                        }
                    }
                    else if (GetAsyncKeyState(VK_LBUTTON) < 0 && !mc->GetLocalPlayer().isUsingItem())
                    {
                        clicker.lclick(mcWindow);
                    }
                }
            }
            lc->vm->DetachCurrentThread();
            FreeLibraryAndExitThread(instance, 0);
            return 0;
        }
        else
        {
            return 0;
        }
    }
}
