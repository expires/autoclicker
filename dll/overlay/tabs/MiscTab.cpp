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
        dirty |= RowCheckbox("Require Sword",  &g_settings.autoblockRequireSword);
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
        ImGui::PopID();

        ImGui::Dummy(ImVec2(0, Theme::M::BodyPad));

        ImGui::PushID("scaffold");
        dirty |= ModuleHeader("Legit Scaffold", &g_settings.scaffoldEnabled, &g_settings.scaffoldKey);
        ImGui::PopID();

        return dirty;
    }
}
