#include "Tabs.h"
#include "../OverlayWidgets.h"
#include "../../config/Settings.h"
#include "../../modules/antievade/AntiEvadeModule.h"
#include "imgui.h"

namespace OverlayTabs
{
    bool RenderMisc()
    {
        using namespace OverlayWidgets;
        bool dirty = false;

        ImGui::PushID("autoblock");
        dirty |= ModuleHeader("Autoblock", &g_settings.autoblockEnabled, &g_settings.autoblockKey);
        if (g_settings.autoblockEnabled) {
            dirty |= RowSlider  ("Delay (ms)",     &g_settings.autoblockDelay,    30, 1000);
            {
                int cooldownSec = g_settings.autoblockCooldown / 1000;
                if (cooldownSec > 30) cooldownSec = 30;
                if (cooldownSec < 0)  cooldownSec = 0;
                if (RowSlider("Cooldown (s)", &cooldownSec, 0, 30, "%ds")) {
                    g_settings.autoblockCooldown = cooldownSec * 1000;
                    dirty = true;
                }
                ImGui::Dummy(ImVec2(0, Theme::M::BodyPad));
            }
        }
        
        ImGui::PopID();

        ImGui::PushID("shiftrecall");
        dirty |= ModuleHeader("Auto Shift Recall", &g_settings.shiftRecallEnabled,
                              &g_settings.shiftRecallKey);
        ImGui::PopID();

        ImGui::PushID("antievade");
        dirty |= ModuleHeader("AntiEvade", &g_settings.antiEvadeEnabled,
                              &g_settings.antiEvadeKey);
        if (g_settings.antiEvadeEnabled) {
            const AntiEvadeModule::DebugState dbg = AntiEvadeModule::Debug();

            ImGui::TextDisabled("%s   ticks %d", dbg.stage.c_str(), dbg.ticks);
            ImGui::TextDisabled("tracked %d   chat %d   %s",
                                dbg.tracked, dbg.chatLines,
                                dbg.holding ? "HOLDING" : "swinging");

            if (dbg.events.empty())
                ImGui::TextDisabled("(no events yet)");

            if (dbg.hovering) {
                ImGui::TextDisabled("%s  leather %d  using %d  sword %d",
                                    dbg.target.empty() ? "?" : dbg.target.c_str(),
                                    dbg.leather ? 1 : 0, dbg.usingItem ? 1 : 0, dbg.sword ? 1 : 0);
                ImGui::TextDisabled("block %dms", dbg.blockMs);
            }
            else {
                ImGui::TextDisabled("no target under crosshair");
            }

            for (const std::string& line : dbg.events)
                ImGui::TextDisabled("%s", line.c_str());

            ImGui::Dummy(ImVec2(0, Theme::M::BodyPad));
        }
        ImGui::PopID();

        ImGui::PushID("scaffold");
        dirty |= ModuleHeader("Legit Scaffold", &g_settings.scaffoldEnabled, &g_settings.scaffoldKey);
        ImGui::PopID();

        ImGui::PushID("sprintreset");
        dirty |= ModuleHeader("Sprint Reset", &g_settings.sprintResetEnabled, &g_settings.sprintResetKey);
        if (g_settings.sprintResetEnabled) {
            int mode = g_settings.sprintResetMode;
            if (RowRadio("Mode", &mode, "W-Tap\0S-Tap\0")) {
                g_settings.sprintResetMode = mode;
                dirty = true;
            }
            dirty |= RowSlider("Post-delay (ms)", &g_settings.sprintResetDelay,    0, 200);
            dirty |= RowSlider("Duration (ms)",   &g_settings.sprintResetDuration, 0, 200);
            ImGui::Dummy(ImVec2(0, Theme::M::BodyPad));
        }
        ImGui::PopID();

        return dirty;
    }
}
