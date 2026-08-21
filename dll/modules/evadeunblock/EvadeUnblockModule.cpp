#include "EvadeUnblockModule.h"
#include "../../config/Settings.h"
#include "../../SDK/Minecraft.h"
#include "../autoclicker/AutoclickerModule.h"
#include "../ModuleCommon.h"
#include "../../logger/Logger.h"
#include "../../overlay/Overlay.h"
#include "Mappings.h"
#include "Platform.h"
#include <cctype>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace EvadeUnblockModule
{
    using Clock   = std::chrono::steady_clock;
    using Instant = Clock::time_point;

    static constexpr auto kPollInterval = std::chrono::milliseconds(50);

    static constexpr int  kChatScan     = 8;
    static constexpr const char* kEvadeMarkers[2] = {
        "you used evade",
        "you failed to evade"
    };

    static int         s_lastChatTime = 0;
    static std::string s_lastChatText;
    static bool        s_primed       = false;

    static bool SelfBlocking(Minecraft& mc)
    {
        Player local = mc.GetLocalPlayer();
        if (local.GetInstance() == nullptr) return false;
        if (!local.isUsingItem()) return false;

        ItemStack held = local.getItemInHand();
        if (held.GetInstance() == nullptr || held.isEmpty()) return false;

        std::string id = held.getDescriptionId();
        if (id.empty()) {
            Component name = held.getHoverName();
            if (name.GetInstance() == nullptr) return false;
            id = name.getString();
        }
        for (char& c : id) c = (char)std::tolower((unsigned char)c);
        return id.find("sword") != std::string::npos;
    }

    static bool SawOwnEvade(Minecraft& mc)
    {
        Gui gui = mc.GetGui();
        if (gui.GetInstance() == nullptr) return false;

        ChatComponent chat = gui.getChat();
        if (chat.GetInstance() == nullptr) return false;

        std::vector<ChatMessage> messages = chat.getRecentMessages(kChatScan);
        if (messages.empty()) return false;

        const int  newest = messages.front().addedTime;
        const bool timed  = newest != 0;

        if (timed && newest < s_lastChatTime) s_primed = false;

        if (!s_primed) {
            s_primed       = true;
            s_lastChatTime = newest;
            s_lastChatText = messages.front().text;
            return false;
        }

        size_t fresh = 0;
        if (timed) {
            while (fresh < messages.size() && messages[fresh].addedTime > s_lastChatTime) ++fresh;
        }
        else {
            while (fresh < messages.size() && messages[fresh].text != s_lastChatText) ++fresh;
        }

        s_lastChatTime = newest;
        s_lastChatText = messages.front().text;

        for (size_t m = 0; m < fresh; ++m) {
            std::string line = messages[m].text;
            for (char& c : line) c = (char)std::tolower((unsigned char)c);

            for (const char* marker : kEvadeMarkers)
                if (line.find(marker) != std::string::npos) return true;
        }
        return false;
    }

    static void ReleaseUseItem(HWND window)
    {
        POINT pt;
        GetCursorPos(&pt);
        SendMessageW(window, WM_RBUTTONUP, 0, MAKELPARAM(pt.x, pt.y));
    }

    DWORD WINAPI init(LPVOID)
    {
        LOG("evadeunblock: thread start");
        if (!ModuleCommon::AttachToJvm()) return 0;
        LOG("evadeunblock: attached; entering loop");

        Minecraft mc;
        HWND      window     = nullptr;
        bool      wasEnabled = false;

        while (!AutoclickerModule::destruct)
        {
            std::this_thread::sleep_for(kPollInterval);

            if (!g_settings.evadeUnblockEnabled) {
                if (wasEnabled) {
                    wasEnabled     = false;
                    s_primed       = false;
                    s_lastChatTime = 0;
                    s_lastChatText.clear();
                }
                continue;
            }
            wasEnabled = true;

            if (window == nullptr) {
                window = FindGameWindow();
                if (window == nullptr) continue;
            }

            if (mc.GetInstance() == nullptr) {
                if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                continue;
            }

            bool unblock = false;
            {
                JLocalFrame frame(64);
                if (!frame.ok()) continue;

                if (SawOwnEvade(mc))
                    unblock = SelfBlocking(mc);

                if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
            }

            if (unblock && GetForegroundWindow() == window && !Overlay::IsMenuVisible())
                ReleaseUseItem(window);
        }

        LOG("evadeunblock: loop exit; detaching");
        lc->vm->DetachCurrentThread();
        return 0;
    }
}
