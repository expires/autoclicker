#include "MacrosModule.h"
#include "../../config/Settings.h"
#include "../../SDK/Minecraft.h"
#include "../../SDK/Capabilities.h"
#include "../autoclicker/AutoclickerModule.h"
#include "../ModuleCommon.h"
#include "../../overlay/Overlay.h"
#include "../../logger/Logger.h"
#include "Platform.h"
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

namespace MacrosModule
{
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
            return ModuleCommon::ContainsCi(name.getString(), m.name);
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

    static void FireDrop(Minecraft& mc, bool entireStack)
    {
        if (lc->env->PushLocalFrame(16) != 0) { lc->env->ExceptionClear(); return; }
        Player player = mc.GetLocalPlayer();
        if (player.GetInstance() != nullptr)
            player.dropSelectedItem(entireStack);
        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        lc->env->PopLocalFrame(nullptr);
    }

    DWORD WINAPI init(LPVOID )
    {
        LOG("macros: thread start");
        if (!ModuleCommon::AttachToJvm()) return 0;
        LOG("macros: attached; entering loop");

        Minecraft  mc;
        HWND       mcWindow = nullptr;

        int  cachedSlot[Settings::MAX_MACROS];
        bool heldPrev  [Settings::MAX_MACROS] = {};
        for (int i = 0; i < Settings::MAX_MACROS; ++i) cachedSlot[i] = -1;

        while (!AutoclickerModule::destruct)
        {

            std::this_thread::sleep_for(std::chrono::milliseconds(33));

            if (mcWindow == nullptr) {
                mcWindow = FindGameWindow();
                if (mcWindow == nullptr) continue;
            }

            if (GetForegroundWindow() != mcWindow) continue;
            if (Overlay::IsMenuVisible()) continue;

            for (int i = 0; i < Settings::MAX_MACROS; ++i) {

                Macro m;
                {
                    std::lock_guard<std::mutex> lk(g_settings.macrosMutex);
                    if (i >= g_settings.macroCount) break;
                    m = g_settings.macros[i];
                }

                if (m.key <= 0 || m.key > 0xFE || m.name[0] == '\0') {
                    heldPrev[i] = false;
                    continue;
                }

                const bool held = (GetAsyncKeyState(m.key) & 0x8000) != 0;
                const bool edge = held && !heldPrev[i];
                heldPrev[i] = held;

                if (edge) cachedSlot[i] = FireMacro(mc, mcWindow, m, cachedSlot[i]);
            }

            if (kNativeDropOverride) {
                static bool dropHeldPrev = false;
                const int  dropKey = g_settings.dropKey;
                if (dropKey > 0 && dropKey <= 0xFE && !Overlay::IsScreenOpen()) {
                    const bool held = (GetAsyncKeyState(dropKey) & 0x8000) != 0;
                    if (held && !dropHeldPrev) {
                        const bool entireStack = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                        FireDrop(mc, entireStack);
                    }
                    dropHeldPrev = held;
                }
                else {
                    dropHeldPrev = false;
                }
            }
        }

        LOG("macros: loop exit; detaching");
        lc->vm->DetachCurrentThread();
        return 0;
    }
}
