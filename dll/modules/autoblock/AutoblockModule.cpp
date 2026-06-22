#include "AutoblockModule.h"
#include "../../config/Settings.h"
#include "../../SDK/Minecraft.h"
#include "Mappings.h"
#include "Platform.h"
#include "../autoclicker/AutoclickerModule.h"
#include "../../overlay/Overlay.h"
#include "../../logger/Logger.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>
#include <thread>

namespace AutoblockModule
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
                    static jclass leClass = nullptr;
                    JClass(leClass, MC_LivingEntity);
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

                        bool isFriend = false;
                        bool listEmpty;
                        {
                            std::lock_guard<std::mutex> lk(g_settings.friendsMutex);
                            listEmpty = g_settings.friends.empty();
                        }
                        if (!sameTeam && !listEmpty) {
                            Component bare = ent.getName();
                            if (bare.GetInstance() != nullptr) {
                                std::string name = bare.getString();
                                for (char& c : name)
                                    c = (char)std::tolower((unsigned char)c);
                                std::lock_guard<std::mutex> lk(g_settings.friendsMutex);
                                for (const auto& f : g_settings.friends)
                                    if (f == name) { isFriend = true; break; }
                            }
                        }

                        result = !sameTeam && !isFriend;
                    }
                }
            }
        }

        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        lc->env->PopLocalFrame(nullptr);
        return result;
    }

    static void RightClick(HWND hwnd)
    {
        POINT pt;
        GetCursorPos(&pt);
        const LPARAM coord = MAKELPARAM(pt.x, pt.y);
        SendMessageW(hwnd, WM_RBUTTONDOWN, MK_RBUTTON, coord);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        SendMessageW(hwnd, WM_RBUTTONUP,   MK_RBUTTON, coord);
    }

    DWORD WINAPI init(LPVOID )
    {
        LOG("autoblock: thread start");
        while (!AutoclickerModule::destruct && !jvmReady())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (AutoclickerModule::destruct) return 0;

        if (lc->vm->AttachCurrentThread(reinterpret_cast<void**>(&lc->env), nullptr) != JNI_OK)
            return 0;
        if (lc->env == nullptr) return 0;
        LOG("autoblock: attached; entering loop");

        Minecraft  mc;
        const HWND mcWindow = FindGameWindow();
        if (mcWindow == nullptr) {
            lc->vm->DetachCurrentThread();
            return 0;
        }

        auto lastAttempt = std::chrono::steady_clock::now() - std::chrono::seconds(10);
        auto lastFire    = std::chrono::steady_clock::now() - std::chrono::seconds(10);

        while (!AutoclickerModule::destruct)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            if (!g_settings.autoblockEnabled)          continue;
            if (Overlay::IsMenuVisible())              continue;
            if (GetForegroundWindow() != mcWindow)     continue;

            {
                const std::string name = SelectedItemName(mc);
                if (!ContainsCi(name, "sword")) continue;
            }

            if (!HoveringLivingEntity(mc)) continue;

            const auto now = std::chrono::steady_clock::now();
            const auto sinceAttempt = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastAttempt).count();
            if (sinceAttempt < g_settings.autoblockDelay) continue;
            lastAttempt = now;

            const auto sinceFire = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFire).count();
            if (sinceFire < g_settings.autoblockCooldown) continue;

            RightClick(mcWindow);
            lastFire = std::chrono::steady_clock::now();
        }

        LOG("autoblock: loop exit; detaching");
        lc->vm->DetachCurrentThread();
        return 0;
    }
}
