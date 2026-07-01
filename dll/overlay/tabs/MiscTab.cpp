#include "Tabs.h"
#include "../OverlayWidgets.h"
#include "../../config/Settings.h"
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
            }
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
        }
        ImGui::PopID();

        return dirty;
    }
}
