#include "MacrosModule.h"
#include "../../Settings.h"
#include "../../SDK/Minecraft.h"
#include "../autoclicker/AutoclickerModule.h"
#include "../../overlay/Overlay.h"
#include "../../Logger.h"
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

    static void SendKey(HWND hwnd, WORD vk)
    {
        UINT  scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
        LPARAM dn  = 1 | (LPARAM(scan) << 16);
        LPARAM up  = 1 | (LPARAM(scan) << 16) | (LPARAM(1) << 30) | (LPARAM(1) << 31);
        SendMessageW(hwnd, WM_KEYDOWN, vk, dn);
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        SendMessageW(hwnd, WM_KEYUP,   vk, up);
    }

    static int FireMacro(Minecraft& mc, HWND hwnd, const Macro& m, int cachedSlot)
    {

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

        if (cachedSlot >= 0 && cachedSlot <= 8 && slotMatches(cachedSlot))
            target = cachedSlot;

        if (target < 0) {
            for (int s = 0; s <= 8; ++s) {
                if (slotMatches(s)) { target = s; break; }
            }
        }

        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        lc->env->PopLocalFrame(nullptr);

        if (target < 0) return -1;

        SendKey(hwnd, (WORD)('1' + target));

        if (m.delay > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(m.delay));

        POINT pt;
        GetCursorPos(&pt);
        SendMessageW(hwnd, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(pt.x, pt.y));
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        SendMessageW(hwnd, WM_RBUTTONUP,   MK_RBUTTON, MAKELPARAM(pt.x, pt.y));

        if (prevSlot >= 0 && prevSlot <= 8 && prevSlot != target) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            SendKey(hwnd, (WORD)('1' + prevSlot));
        }

        return target;
    }

    DWORD WINAPI init(LPVOID )
    {
        AC_LOG("macros: thread start");
        while (!AutoclickerModule::destruct && !jvmReady())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (AutoclickerModule::destruct) return 0;

        if (lc->vm->AttachCurrentThread(reinterpret_cast<void**>(&lc->env), nullptr) != JNI_OK)
            return 0;
        if (lc->env == nullptr) return 0;
        AC_LOG("macros: attached; entering loop");

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

            std::this_thread::sleep_for(std::chrono::milliseconds(33));

            if (GetForegroundWindow() != mcWindow) continue;
            if (Overlay::IsMenuVisible()) continue;

            const int count = g_settings.macroCount;
            for (int i = 0; i < count && i < Settings::MAX_MACROS; ++i) {

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

        AC_LOG("macros: loop exit; detaching");
        lc->vm->DetachCurrentThread();
        return 0;
    }
}
