#include "AutoclickerModule.h"
#include "../../Settings.h"
#include "../../network/Network.h"
#include "Config.h"
#include <chrono>
#include <string>

namespace AutoclickerModule
{
    Clicker clicker(12);
    std::atomic<bool> destruct(false);

    // Poll the local player for username + UUID, firing the ban check on
    // every successful read and the report webhook only when the username
    // changes (or on first sight). Designed to be called repeatedly from
    // the autoclicker loop — every JNI step is guarded so calling on the
    // main menu (player == null), during world load (player exists but
    // name not populated yet), or after a JNI exception is harmless: we
    // just bail and the caller retries on the next tick. ExceptionClear
    // after every JNI call to prevent a pending exception from a partial
    // read leaking into the next call and aborting the JVM.
    static void PollUser(Minecraft& mc, std::string& lastSeenUsername)
    {
        if (lc->env == nullptr) return;
        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

        // Minecraft.instance can be null very early in the JVM bootstrap,
        // before MC's main() has run setInstance(). The class load itself
        // is enough for our `lc->GetClass(MC_Minecraft)` lookup to return
        // non-null, so we can hit this point and still get null here.
        jobject mcInst = mc.GetInstance();
        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        if (mcInst == nullptr) return;

        // Minecraft.player is null on the title screen, world-select, and
        // during the initial world-load phase. Bail; we'll retry in 30s.
        Player player = mc.GetLocalPlayer();
        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        if (player.GetInstance() == nullptr) return;

        Component name = player.getName();
        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        if (name.GetInstance() == nullptr) return;

        const std::string username = name.getString();
        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        if (username.empty()) return;

        // UUID can briefly be null in early world-join (Player exists but
        // GameProfile hasn't been wired up yet). Empty-string is the
        // documented "no uuid" sentinel for the webhook payload.
        const std::string uuid = player.getUUID();
        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

        const bool isNew = (username != lastSeenUsername);
        if (isNew) lastSeenUsername = username;

        // Off-thread so a slow GitHub/Discord round-trip doesn't stall the
        // autoclicker tick. The capture-by-value keeps the strings alive
        // for the lifetime of the lambda even if PollUser returns first.
        std::thread([username, uuid, isNew]() {
            if (!uuid.empty() && Network::IsBanned(uuid)) {
                destruct = true;
                return;
            }
            if (isNew)
                Network::ReportUser(username, uuid.empty() ? "no-uuid" : uuid);
        }).detach();
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
            // CPS comes from g_settings (loaded in DllMain). The legacy
            // ac_config.json sidecar file used to live here and would
            // overwrite g_settings.cps with a default of 12 on every inject,
            // wiping whatever the user had set in the menu the previous
            // session. Removed — the Settings save/load path is the single
            // source of truth now.
            clicker.setCPS(g_settings.cps);

            lc->GetLoadedClasses();

            const auto mc = std::make_unique<Minecraft>();
            const HWND mcWindow = FindWindowW(L"GLFW30", nullptr);

            // First poll runs on the first tick (lastUserPoll = far past),
            // then every 30s. We keep polling for the lifetime of the DLL
            // so an account switch mid-session (logout + log back in as a
            // different user, or a renamed account on reconnect) re-fires
            // the webhook with the new identity.
            std::string lastSeenUsername;
            auto lastUserPoll = std::chrono::steady_clock::now()
                              - std::chrono::seconds(31);

            while (!destruct)
            {
                const auto now = std::chrono::steady_clock::now();
                if (now - lastUserPoll >= std::chrono::seconds(30)) {
                    lastUserPoll = now;
                    PollUser(*mc, lastSeenUsername);
                }

                const HWND activeWindow = GetForegroundWindow();
                clicker.setCPS(g_settings.cps);
                DELAY(TICK);

                while (!destruct && activeWindow == mcWindow && GetAsyncKeyState(VK_LBUTTON) && g_settings.acEnabled)
                {
                    // Self-destruct is now driven entirely off the shared
                    // selfDestruct flag — the overlay's edge-detected
                    // selfDestructKey handler and the in-menu Self-Destruct
                    // button both set it. END is the default binding so the
                    // historical behavior is preserved without a hard-coded
                    // VK check here.
                    if (g_settings.selfDestruct)
                    {
                        destruct = true;
                        break;
                    }

                    // Defensive chain. The unguarded chained accessors that
                    // used to live here would dereference null jobjects on the
                    // main menu / during world load (where Minecraft.screen
                    // can be null, gameMode is null pre-world, hitResult is
                    // null before the player exists, and localPlayer is null
                    // outside a world). Each step caches the jobject, null-
                    // checks it, and clears any pending JNI exception before
                    // moving on so a partial-state read can't poison the next.
                    if (mc->GetInstance() == nullptr) {
                        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                        break;
                    }

                    {
                        Screen screen = mc->GetScreen();
                        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                        if (screen.GetInstance() != nullptr) {
                            const bool isPause   = screen.isPauseScreen();
                            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                            const bool closeOnEsc = screen.shouldCloseOnEsc();
                            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                            if (isPause || closeOnEsc) break;
                        }
                    }

                    MultiPlayerGameMode gm = mc->GetMultiPlayerGameMode();
                    if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                    HitResult hr = mc->getHitResult();
                    if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

                    // -1 sentinels for unavailable: keeps the original
                    // `playerMode != 2` and `hitType == 1` semantics so the
                    // block-break path stays gated by real game state.
                    const int playerMode = (gm.GetInstance() != nullptr) ? gm.getPlayerMode() : -1;
                    if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                    const int hitType    = (hr.GetInstance() != nullptr) ? hr.getType()       : -1;
                    if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

                    if (g_settings.breakBlocks && playerMode != 2 && hitType == 1)
                    {
                        bool hasClickedBlock = false;

                        while (GetAsyncKeyState(VK_LBUTTON))
                        {
                            // Re-read every iteration — the player can swap
                            // out of the block face mid-hold.
                            MultiPlayerGameMode gm2 = mc->GetMultiPlayerGameMode();
                            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                            HitResult hr2 = mc->getHitResult();
                            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

                            const int pm = (gm2.GetInstance() != nullptr) ? gm2.getPlayerMode() : -1;
                            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                            const int ht = (hr2.GetInstance() != nullptr) ? hr2.getType()       : -1;
                            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

                            if (pm == 2 || ht != 1) break;

                            if (!hasClickedBlock && clicker.getClicksPerSecond() > 0)
                            {
                                hasClickedBlock = true;
                                clicker.mouseDown(mcWindow);
                            }

                            DELAY(clicker.randomDelay(1.0));
                        }
                    }
                    else if (GetAsyncKeyState(VK_LBUTTON) < 0)
                    {
                        Player lp = mc->GetLocalPlayer();
                        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                        // No player → no useItem state, fall through to the
                        // click as if nothing's blocking it (matches the
                        // existing "click while not using item" intent).
                        bool usingItem = false;
                        if (lp.GetInstance() != nullptr) {
                            usingItem = lp.isUsingItem();
                            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                        }
                        if (!usingItem) clicker.lclick(mcWindow);
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
