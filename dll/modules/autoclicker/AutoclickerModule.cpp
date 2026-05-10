#include "AutoclickerModule.h"
#include "../../Settings.h"
#include "../../network/Network.h"
#include "Config.h"

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
            const int cps = loadCPS(instance);
            g_settings.cps = cps;
            clicker.setCPS(cps);

            lc->GetLoadedClasses();

            const auto mc = std::make_unique<Minecraft>();
            const HWND mcWindow = FindWindowW(L"GLFW30", nullptr);

            bool userChecked = false;

            while (!destruct)
            {
                if (!userChecked && mc->GetInstance() != nullptr)
                {
                    Player player = mc->GetLocalPlayer();
                    if (player.GetInstance() != nullptr)
                    {
                        FILE* log;
                        fopen_s(&log, "C:\\Users\\Public\\ac_debug.log", "a");

                        Component name = player.getName();
                        if (log) fprintf(log, "name instance: %s\n", name.GetInstance() ? "ok" : "null");

                        if (name.GetInstance() != nullptr)
                        {
                            std::string username = name.getString();
                            if (log) fprintf(log, "username: '%s'\n", username.c_str());

                            if (!username.empty())
                            {
                                userChecked = true;
                                if (log) { fprintf(log, "launching thread\n"); fclose(log); log = nullptr; }
                                std::thread([username]() {
                                    FILE* tlog;
                                    fopen_s(&tlog, "C:\\Users\\Public\\ac_debug.log", "a");
                                    if (tlog) fprintf(tlog, "checking ban...\n");
                                    if (tlog) {
                                        std::wstring bp(GITHUB_BANNED_PATH);
                                        std::string nbp(bp.begin(), bp.end());
                                        fprintf(tlog, "banned path: '%s'\n", nbp.c_str());
                                        fflush(tlog);
                                    }

                                    bool banned = Network::IsBanned(username);
                                    if (tlog) fprintf(tlog, "ban result: %s\n", banned ? "banned" : "not banned");

                                    if (!banned) {
                                        if (tlog) {
                                            std::wstring wp(DISCORD_WEBHOOK_PATH);
                                            std::string npath(wp.begin(), wp.end());
                                            fprintf(tlog, "webhook path: '%s'\n", npath.c_str());
                                            fflush(tlog);
                                        }
                                        if (tlog) fprintf(tlog, "sending report...\n");
                                        Network::ReportUser(username);
                                        if (tlog) fprintf(tlog, "report done\n");
                                    }

                                    if (tlog) fclose(tlog);
                                }).detach();
                            }
                        }

                        if (log) { fclose(log); log = nullptr; }
                        lc->env->ExceptionClear();
                    }
                }

                const HWND activeWindow = GetForegroundWindow();
                clicker.setCPS(g_settings.cps);
                DELAY(TICK);

                while (activeWindow == mcWindow && GetAsyncKeyState(VK_LBUTTON) && g_settings.acEnabled)
                {
                    if (GetAsyncKeyState(VK_END) || g_settings.selfDestruct)
                    {
                        destruct = true;
                        break;
                    }

                    if (mc->GetScreen().isPauseScreen() || mc->GetScreen().shouldCloseOnEsc())
                        break;

                    if (g_settings.breakBlocks && mc->GetMultiPlayerGameMode().getPlayerMode() != 2 && mc->getHitResult().getType() == 1)
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
