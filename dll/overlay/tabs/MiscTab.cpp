#include "Tabs.h"
#include "../OverlayWidgets.h"
#include "../../Settings.h"
#include "imgui.h"

namespace OverlayTabs
{
    bool RenderMisc()
    {
        using namespace OverlayWidgets;
        bool dirty = false;

        ImGui::PopStyleVar();
        ImGui::PushStyleColor(ImGuiCol_Text, FromHex(0xf2c14e));
        ImGui::TextUnformatted("EXPERIMENTAL");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, FromHex(0x707a8c));
        ImGui::TextWrapped("Right-click ability exploits.");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 6));

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        ImGui::PushID("autoblock");
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
        ImGui::TextUnformatted("Autoblock");
        ImGui::PopFont();
        dirty |= RowCheckbox("Enabled",        &g_settings.autoblockEnabled);
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

        return dirty;
    }
}
