#include "MacrosModule.h"
#include "../../Settings.h"
#include "../../SDK/Minecraft.h"
#include "../autoclicker/AutoclickerModule.h"
#include "../../overlay/Overlay.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>
#include <thread>

namespace MacrosModule
{
    static bool jvmReady()
    {
        return lc->vm != nullptr && lc->classesLoaded.load(std::memory_order_acquire);
    }

    // Case-insensitive substring match. Empty needle matches nothing — we
    // don't want a macro with an unset name to suddenly trigger on every
    // hotbar item.
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

    // Press-and-release a key into MC's window. Builds lParam exactly the way
    // a real keystroke would — scan code + repeat=1 + up-edge bits on release
    // — so GLFW's win32 backend (and therefore MC's KeyboardHandler) sees a
    // genuine key event and emits the matching ServerboundSetCarriedItemPacket
    // for hotbar number keys. Direct field-writing Inventory.selected would
    // change the held item client-side but never tell the server, so the
    // right-click that follows would use the wrong item server-side.
    static void SendKey(HWND hwnd, WORD vk)
    {
        UINT  scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
        LPARAM dn  = 1 | (LPARAM(scan) << 16);
        LPARAM up  = 1 | (LPARAM(scan) << 16) | (LPARAM(1) << 30) | (LPARAM(1) << 31);
        SendMessageW(hwnd, WM_KEYDOWN, vk, dn);
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        SendMessageW(hwnd, WM_KEYUP,   vk, up);
    }

    // Run one macro: find slot (cache-first), switch, wait, right-click,
    // restore. Returns the slot the item was found in (or -1 if not present),
    // which the caller stores back into the cache.
    static int FireMacro(Minecraft& mc, HWND hwnd, const Macro& m, int cachedSlot)
    {
        // Bound the local refs we create here so the JNI frame stays clean
        // across many fires.
        if (lc->env->PushLocalFrame(64) != 0) {
            lc->env->ExceptionClear();
            return cachedSlot;
        }

        Player player = mc.GetLocalPlayer();
        if (player.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); return cachedSlot; }

        Inventory inv = player.getInventory();
        if (inv.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); return cachedSlot; }

        const int prevSlot = inv.getSelected();
        int       target   = -1;

        auto slotMatches = [&](int s) -> bool {
            ItemStack stack = inv.getItem(s);
            if (stack.GetInstance() == nullptr || stack.isEmpty()) return false;
            Component name = stack.getHoverName();
            if (name.GetInstance() == nullptr) return false;
            return ContainsCi(name.getString(), m.name);
        };

        // Cache-first: trust the cached slot only if it still holds an item
        // whose hover name still matches. Anything else (slot emptied, item
        // moved, item renamed) falls through to a full hotbar rescan.
        if (cachedSlot >= 0 && cachedSlot <= 8 && slotMatches(cachedSlot))
            target = cachedSlot;

        if (target < 0) {
            for (int s = 0; s <= 8; ++s) {
                if (slotMatches(s)) { target = s; break; }
            }
        }

        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        lc->env->PopLocalFrame(nullptr);

        if (target < 0) return -1; // No matching item — invalidate cache.

        // Switch to the macro slot. SendKey is synchronous (SendMessage), so
        // by the time it returns MC's WndProc has processed the key — but the
        // game itself only sets Inventory.selected on its tick, hence the
        // configurable post-switch delay below before we send the right-click.
        SendKey(hwnd, (WORD)('1' + target));

        if (m.delay > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(m.delay));

        // Right-click once. Mirrors Clicker::rclick — short hold between
        // down/up so MC registers a clean press rather than a flicker.
        POINT pt;
        GetCursorPos(&pt);
        SendMessageW(hwnd, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(pt.x, pt.y));
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        SendMessageW(hwnd, WM_RBUTTONUP,   MK_RBUTTON, MAKELPARAM(pt.x, pt.y));

        // Restore the player's previous selection. Skip the round-trip if the
        // macro was already on the held slot.
        if (prevSlot >= 0 && prevSlot <= 8 && prevSlot != target) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            SendKey(hwnd, (WORD)('1' + prevSlot));
        }

        return target;
    }

    DWORD WINAPI init(LPVOID /*lpParam*/)
    {
        // Wait for the autoclicker thread to attach + populate the class map.
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

        int  cachedSlot[Settings::MAX_MACROS];
        bool heldPrev  [Settings::MAX_MACROS] = {};
        for (int i = 0; i < Settings::MAX_MACROS; ++i) cachedSlot[i] = -1;

        while (!AutoclickerModule::destruct)
        {
            // 30Hz poll: fast enough to feel responsive, slow enough that the
            // hotbar scan + a few JNI calls per held key won't burn measurable
            // CPU.
            std::this_thread::sleep_for(std::chrono::milliseconds(33));

            // Don't fire while the player isn't actually in MC (alt-tabbed,
            // typing in the overlay, etc.) — would surprise users with random
            // slot switches.
            if (GetForegroundWindow() != mcWindow) continue;
            if (Overlay::IsMenuVisible()) continue;

            // Only walk the active prefix. macroCount is written from the
            // overlay thread when the user adds/removes; reading it racily
            // is fine — a one-frame mismatch just means we skip a brand-new
            // empty macro (no key set yet anyway) or fire one extra time on
            // an entry whose row was just deleted (worst case: switches to a
            // slot the user no longer wants once).
            const int count = g_settings.macroCount;
            for (int i = 0; i < count && i < Settings::MAX_MACROS; ++i) {
                // Snapshot once so any concurrent edit in the overlay can't
                // change the key mid-evaluation.
                const Macro m = g_settings.macros[i];

                if (m.key <= 0 || m.key > 0xFE || m.name[0] == '\0') {
                    heldPrev[i] = false;
                    continue;
                }

                const bool held = (GetAsyncKeyState(m.key) & 0x8000) != 0;
                const bool edge = held && !heldPrev[i];
                heldPrev[i] = held;

                if (edge) cachedSlot[i] = FireMacro(mc, mcWindow, m, cachedSlot[i]);
            }
        }

        lc->vm->DetachCurrentThread();
        return 0;
    }
}
