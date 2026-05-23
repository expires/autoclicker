#include "AutoAbilityModule.h"
#include "../../Settings.h"
#include "../../SDK/Minecraft.h"
#include "../../SDK/Mappings.h"
#include "../autoclicker/AutoclickerModule.h"
#include "../../overlay/Overlay.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>
#include <thread>

namespace AutoAbilityModule
{
    static bool jvmReady()
    {
        return lc->vm != nullptr && lc->classesLoaded.load(std::memory_order_acquire);
    }

    static bool ContainsCi(const std::string& hay, const char* needle)
    {
        if (!needle || !*needle) return false;
        std::string h = hay;
        std::string n = needle;
        std::transform(h.begin(), h.end(), h.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });
        std::transform(n.begin(), n.end(), n.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });
        return h.find(n) != std::string::npos;
    }

    // Display name of the selected hotbar slot. Empty on any failure — caller
    // treats as "not a sword" so the safer default is no fire.
    static std::string SelectedItemName(Minecraft& mc)
    {
        if (lc->env->PushLocalFrame(32) != 0) {
            lc->env->ExceptionClear();
            return {};
        }
        std::string out;
        Player player = mc.GetLocalPlayer();
        if (player.GetInstance() != nullptr) {
            Inventory inv = player.getInventory();
            if (inv.GetInstance() != nullptr) {
                const int sel = inv.getSelected();
                if (sel >= 0 && sel <= 8) {
                    ItemStack stack = inv.getItem(sel);
                    if (stack.GetInstance() != nullptr && !stack.isEmpty()) {
                        Component name = stack.getHoverName();
                        if (name.GetInstance() != nullptr)
                            out = name.getString();
                    }
                }
            }
        }
        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        lc->env->PopLocalFrame(nullptr);
        return out;
    }

    // True if the crosshair is on a hostile LivingEntity — a player/mob, not
    // an item drop / arrow / xp orb, and not on the local player's scoreboard
    // team. Right-clicking a non-living entity wouldn't trigger any sword
    // ability anyway; the team check mirrors the same scoreboard-team
    // ref-equality test AimAssist uses so the user can't accidentally land
    // an ability on a clanmate. "No team" never counts as an alliance — same
    // policy as vanilla MC.
    static bool HoveringLivingEntity(Minecraft& mc)
    {
        bool result = false;

        if (lc->env->PushLocalFrame(16) != 0) {
            lc->env->ExceptionClear();
            return false;
        }

        HitResult hr = mc.getHitResult();
        if (hr.GetInstance() != nullptr && hr.getType() == 2) {
            EntityHitResult ehr = hr.getEntityHitResult();
            if (ehr.GetInstance() != nullptr) {
                Entity ent = ehr.getEntity();
                if (ent.GetInstance() != nullptr) {
                    jclass leClass = lc->GetClass(MC_LivingEntity);
                    if (leClass != nullptr &&
                        lc->env->IsInstanceOf(ent.GetInstance(), leClass) == JNI_TRUE)
                    {
                        bool sameTeam = false;
                        Player local = mc.GetLocalPlayer();
                        if (local.GetInstance() != nullptr) {
                            jobject myTeam = local.getTeamRaw();
                            if (myTeam != nullptr) {
                                jobject theirTeam = ent.getTeamRaw();
                                if (theirTeam != nullptr) {
                                    sameTeam = (lc->env->IsSameObject(theirTeam, myTeam) == JNI_TRUE);
                                    lc->env->DeleteLocalRef(theirTeam);
                                }
                                lc->env->DeleteLocalRef(myTeam);
                            }
                        }
                        result = !sameTeam;
                    }
                }
            }
        }

        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        lc->env->PopLocalFrame(nullptr);
        return result;
    }

    // Synthetic right-click matching LeapModule/macros — 30ms hold is short
    // enough to feel like one press, long enough for MC's tick to see the
    // down→up pair reliably.
    static void RightClick(HWND hwnd)
    {
        POINT pt;
        GetCursorPos(&pt);
        const LPARAM coord = MAKELPARAM(pt.x, pt.y);
        SendMessageW(hwnd, WM_RBUTTONDOWN, MK_RBUTTON, coord);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        SendMessageW(hwnd, WM_RBUTTONUP,   MK_RBUTTON, coord);
    }

    DWORD WINAPI init(LPVOID /*lpParam*/)
    {
        // Wait for AC to attach + populate the class map.
        while (!AutoclickerModule::destruct && !jvmReady())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (AutoclickerModule::destruct) return 0;

        if (lc->vm->AttachCurrentThread(reinterpret_cast<void**>(&lc->env), nullptr) != JNI_OK)
            return 0;
        if (lc->env == nullptr) return 0;

        Minecraft  mc;
        const HWND mcWindow = FindWindowW(L"GLFW30", nullptr);
        if (mcWindow == nullptr) {
            lc->vm->DetachCurrentThread();
            return 0;
        }

        // Both timers start "long ago" so the very first valid attempt fires
        // without waiting out a delay/cooldown the user never set up.
        auto lastAttempt = std::chrono::steady_clock::now() - std::chrono::seconds(10);
        auto lastFire    = std::chrono::steady_clock::now() - std::chrono::seconds(10);

        while (!AutoclickerModule::destruct)
        {
            // 20Hz poll — same cadence as the leap module. Hit-result reads
            // are cheap; this is well under MC's per-tick interaction budget.
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            if (!g_settings.autoAbilityEnabled)        continue;
            if (Overlay::IsMenuVisible())              continue;
            if (GetForegroundWindow() != mcWindow)     continue;

            if (g_settings.autoAbilityKey > 0 && g_settings.autoAbilityKey <= 0xFE) {
                if (!(GetAsyncKeyState(g_settings.autoAbilityKey) & 0x8000))
                    continue;
            }

            if (g_settings.autoAbilityRequireSword) {
                const std::string name = SelectedItemName(mc);
                if (!ContainsCi(name, "sword")) continue;
            }

            if (!HoveringLivingEntity(mc)) continue;

            const auto now = std::chrono::steady_clock::now();
            const auto sinceAttempt = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastAttempt).count();
            if (sinceAttempt < g_settings.autoAbilityDelay) continue;
            lastAttempt = now;

            const auto sinceFire = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFire).count();
            if (sinceFire < g_settings.autoAbilityCooldown) continue;

            RightClick(mcWindow);
            lastFire = std::chrono::steady_clock::now();
        }

        lc->vm->DetachCurrentThread();
        return 0;
    }
}
