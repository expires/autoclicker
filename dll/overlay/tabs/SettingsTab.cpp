#include "Tabs.h"
#include "../OverlayWidgets.h"
#include "../../config/Settings.h"
#include "../../modules/esp/EspModule.h"
#include "imgui.h"
#include <memory>

namespace OverlayTabs
{
    bool RenderSettings()
    {
        using namespace OverlayWidgets;
        bool dirty = false;
        
        ImGui::PushID("keybinds");
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
        ImGui::TextUnformatted("Keybinds");
        ImGui::PopFont();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, Theme::M::KeybindGap));
        dirty |= RowKeybind("Menu",                    &g_settings.menuKey);
        dirty |= RowKeybind("Self-Destruct",   &g_settings.selfDestructKey);
        dirty |= RowKeybind("Autoclicker",              &g_settings.acKey);
        dirty |= RowKeybind("Aim Assist",              &g_settings.aimKey);
        dirty |= RowKeybind("ESP",                     &g_settings.espKey);
        ImGui::PopStyleVar();

        ImGui::PopID();

        ImGui::Dummy(ImVec2(0, Theme::M::BodyPad));
        ImGui::TextDisabled("Source & updates:");
        ImGui::SameLine(0, Theme::M::ListGap);
        ImGui::TextLinkOpenURL("github.com/expires/autoclicker",
                               "https://github.com/expires/autoclicker");

        return dirty;
    }
}