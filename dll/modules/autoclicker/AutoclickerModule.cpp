#include "AutoclickerModule.h"
#include "../../Settings.h"
#include "../../Log.h"
#include <cmath>

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

    // java/util/List interface methods — looked up once and cached as globals.
    static jmethodID s_listSize = nullptr;
    static jmethodID s_listGet  = nullptr;

    static void initListMethods()
    {
        if (s_listSize != nullptr) return;
        jclass listClass = lc->env->FindClass("java/util/List");
        if (listClass == nullptr || lc->env->ExceptionCheck())
        {
            lc->env->ExceptionClear();
            McBotLog("initListMethods: FindClass java/util/List failed");
            return;
        }
        s_listSize = lc->env->GetMethodID(listClass, "size", "()I");
        s_listGet  = lc->env->GetMethodID(listClass, "get",  "(I)Ljava/lang/Object;");
        lc->env->DeleteLocalRef(listClass);
        if (s_listSize == nullptr || s_listGet == nullptr)
        {
            lc->env->ExceptionClear();
            McBotLog("initListMethods: GetMethodID failed");
        }
    }

    // Collect current game state into g_gameState for the LLM thread to read.
    // Must be called from the autoclicker thread (the only thread with a valid lc->env).
    static void collectGameState(Minecraft* mc)
    {
        // Clear any stale exception before touching JNI
        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

        GameState gs = {};

        HitResult hit = mc->getHitResult();
        if (hit.GetInstance() != nullptr && hit.getType() == 2)
        {
            gs.entityInCrosshair = true;
            EntityHitResult ehr = hit.getEntityHitResult();
            if (ehr.GetInstance() != nullptr)
            {
                Entity ent = ehr.getEntity();
                if (ent.GetInstance() != nullptr)
                {
                    Component typeName = ent.getTypeName();
                    std::string name = typeName.getString();
                    strncpy_s(gs.entityType, name.c_str(), sizeof(gs.entityType) - 1);
                    typeName.Cleanup();
                }
            }
        }

        double px = 0.0, pz = 0.0;
        Player player = mc->GetLocalPlayer();
        if (player.GetInstance() != nullptr)
        {
            gs.usingItem = player.isUsingItem();
            gs.health    = player.getHealth();
            gs.maxHealth = player.getMaxHealth();
            px = player.getX();
            pz = player.getZ();

            ItemStack is = player.getItemInHand();
            if (is.GetInstance() != nullptr)
            {
                Item item = is.getItem();
                if (item.GetInstance() != nullptr)
                {
                    Component itemName = item.getName(is.GetInstance());
                    std::string name = itemName.getString();
                    strncpy_s(gs.heldItem, name.c_str(), sizeof(gs.heldItem) - 1);
                    itemName.Cleanup();
                }
            }
        }

        // Scan nearby players via Level.players()
        initListMethods();
        if (s_listSize != nullptr && s_listGet != nullptr)
        {
            Level level = mc->GetLevel();
            if (level.GetInstance() != nullptr)
            {
                jobject playerList = level.players();
                if (playerList != nullptr)
                {
                    jint count = lc->env->CallIntMethod(playerList, s_listSize);
                    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); count = 0; }

                    for (jint i = 0; i < count && gs.nearbyCount < GameState::MAX_NEARBY; ++i)
                    {
                        jobject entObj = lc->env->CallObjectMethod(playerList, s_listGet, i);
                        if (entObj == nullptr || lc->env->ExceptionCheck())
                        {
                            lc->env->ExceptionClear();
                            continue;
                        }

                        Entity ent(entObj);
                        double ex = ent.getX(), ez = ent.getZ();
                        float dist = static_cast<float>(sqrt((ex - px) * (ex - px) + (ez - pz) * (ez - pz)));

                        if (dist >= 0.5f) // skip local player
                        {
                            Component nameComp = ent.getName();
                            std::string nameStr = nameComp.getString();
                            nameComp.Cleanup();

                            auto& slot = gs.nearby[gs.nearbyCount++];
                            strncpy_s(slot.name, nameStr.c_str(), sizeof(slot.name) - 1);
                            slot.distanceXZ = dist;
                        }

                        lc->env->DeleteLocalRef(entObj);
                    }
                    lc->env->DeleteLocalRef(playerList);
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(g_gameStateMutex);
            g_gameState = gs;
        }
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

            while (!destruct)
            {
                const HWND activeWindow = GetForegroundWindow();
                clicker.setCPS(g_settings.cps);
                DELAY(TICK);

                // LLM-driven action (fires independently of manual LMB hold)
                if (g_settings.llmEnabled && activeWindow == mcWindow)
                {
                    collectGameState(mc.get());

                    if (!mc->GetScreen().isPauseScreen() && !mc->GetScreen().shouldCloseOnEsc())
                    {
                        BotAction action = g_botAction.load();
                        if (action == BotAction::ATTACK)
                        {
                            if (mc->getHitResult().getType() == 2 && !mc->GetLocalPlayer().isUsingItem())
                                clicker.lclick(mcWindow);
                        }
                        else if (action == BotAction::USE_ITEM)
                        {
                            clicker.rclick(mcWindow);
                        }
                    }
                }

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
