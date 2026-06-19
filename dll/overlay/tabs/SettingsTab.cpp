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
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, Theme::M::KeybindGap));
        dirty |= RowKeybind("Menu",                    &g_settings.menuKey);
        dirty |= RowKeybind("Self-Destruct",   &g_settings.selfDestructKey);
        ImGui::PopStyleVar();
        ImGui::PopID();

        return dirty;
    }
}