#include "FriendsModule.h"
#include "../../Settings.h"
#include "../../SDK/Minecraft.h"
#include "Mappings.h"
#include "../autoclicker/AutoclickerModule.h"
#include "../../overlay/Overlay.h"
#include <cctype>
#include <chrono>
#include <string>
#include <thread>

namespace FriendsModule
{
    static bool jvmReady()
    {
        return lc->vm != nullptr && lc->classesLoaded.load(std::memory_order_acquire);
    }

    // Bare GameProfile username of whatever the crosshair is on, lowercased
    // for case-insensitive friend matching. Empty if there's no hovered entity
    // or it isn't a Player — caller treats empty as "nothing to toggle".
    static std::string HoveredPlayerName(Minecraft& mc)
    {
        std::string out;

        if (lc->env->PushLocalFrame(16) != 0) {
            lc->env->ExceptionClear();
            return out;
        }

        HitResult hr = mc.getHitResult();
        if (hr.GetInstance() != nullptr && hr.getType() == 2) {
            EntityHitResult ehr = hr.getEntityHitResult();
            if (ehr.GetInstance() != nullptr) {
                Entity ent = ehr.getEntity();
                if (ent.GetInstance() != nullptr) {
                    // Restrict to actual players — toggling a friend on a
                    // squid or item drop would just litter the friends list
                    // with junk names. LocalPlayer/RemotePlayer both extend
                    // AbstractClientPlayer.
                    jclass apClass = lc->GetClass(MC_AbstractClientPlayer);
                    if (apClass != nullptr &&
                        lc->env->IsInstanceOf(ent.GetInstance(), apClass) == JNI_TRUE)
                    {
                        Component bare = ent.getName();
                        if (bare.GetInstance() != nullptr) {
                            out = bare.getString();
                            for (char& c : out)
                                c = (char)std::tolower((unsigned char)c);
                        }
                    }
                }
            }
        }

        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        lc->env->PopLocalFrame(nullptr);
        return out;
    }

    // Add `name` if it isn't already on the list, remove it if it is. Returns
    // true if a write happened so the caller knows to Save(). All friends-list
    // mutations from this module funnel through this so the mutex hold is
    // short and the de-dup logic lives in one place.
    static bool ToggleFriend(const std::string& name)
    {
        if (name.empty()) return false;

        bool changed = false;
        {
            std::lock_guard<std::mutex> lk(g_settings.friendsMutex);
            for (auto it = g_settings.friends.begin(); it != g_settings.friends.end(); ++it) {
                if (*it == name) {
                    g_settings.friends.erase(it);
                    changed = true;
                    break;
                }
            }
            if (!changed) {
                g_settings.friends.push_back(name);
                changed = true;
            }
        }
        return changed;
    }

    DWORD WINAPI init(LPVOID /*lpParam*/)
    {
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

        // Edge-trigger state. We poll at 20Hz which is fast enough that a
        // human tap (~50-150ms held) reliably catches a transition; faster
        // polling would just burn cycles. `prevDown` is the key state from
        // the last iteration so we can distinguish "pressed this frame"
        // from "still held down" — only the rising edge triggers a toggle.
        bool prevDown = false;

        while (!AutoclickerModule::destruct)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            const int key = g_settings.friendKey;
            // Unbound → no-op; resync prevDown so a future re-bind doesn't
            // pick up a stale "already pressed" from a key that's been held
            // continuously since before binding.
            if (key <= 0 || key > 0xFE) { prevDown = false; continue; }

            // Gating: respect MC focus + overlay menu state. The menu's
            // text inputs (Friends tab name field) should be able to take
            // keystrokes without us double-handling them as a toggle.
            if (GetForegroundWindow() != mcWindow) { prevDown = false; continue; }
            if (Overlay::IsMenuVisible())          { prevDown = false; continue; }

            const bool downNow = (GetAsyncKeyState(key) & 0x8000) != 0;

            if (downNow && !prevDown) {
                const std::string name = HoveredPlayerName(mc);
                if (!name.empty()) {
                    if (ToggleFriend(name)) g_settings.Save();
                }
            }

            prevDown = downNow;
        }

        lc->vm->DetachCurrentThread();
        return 0;
    }
}
