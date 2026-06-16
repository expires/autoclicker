#include "Tabs.h"
#include "../OverlayWidgets.h"
#include "../../Settings.h"
#include "../../modules/esp/EspModule.h"
#include "imgui.h"
#include <memory>

namespace OverlayTabs
{
    bool RenderSettings()
    {
        using namespace OverlayWidgets;
        bool dirty = false;

        dirty |= RowKeybind("Menu Key",          &g_settings.menuKey);
        dirty |= RowKeybind("Self-Destruct Key", &g_settings.selfDestructKey);

        std::shared_ptr<const EspModule::Snapshot> snapPtr = EspModule::Acquire();
        EspModule::Snapshot snap = snapPtr ? *snapPtr : EspModule::Snapshot{};

        ImGui::PushStyleColor(ImGuiCol_Text, FromHex(Theme::TextDim));
        ImGui::Text("valid=%d  mc=%d  lp=%d  lvl=%d  gr=%d  cam=%d",
            snap.valid, snap.gotMinecraft, snap.gotLocalPlayer,
            snap.gotLevel, snap.gotGameRenderer, snap.gotCamera);
        ImGui::Text("players()=%d  targets=%d",
            snap.rawPlayerCount, (int)snap.targets.size());
        ImGui::Text("cam=(%.1f,%.1f,%.1f)  yaw=%.1f  pitch=%.1f  fov=%.1f",
            snap.cam.x, snap.cam.y, snap.cam.z,
            snap.cam.yRot, snap.cam.xRot, snap.cam.fov);
        ImGui::PopStyleColor();

        ImGui::PopStyleVar();
        ImGui::Dummy(ImVec2(0, 8));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,        FromHex(Theme::Danger));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, FromHex(Theme::DangerHovered));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  FromHex(Theme::DangerActive));
        ImGui::PushStyleColor(ImGuiCol_Text,          FromHex(Theme::White));
        if (ImGui::Button("Self-Destruct",
                ImVec2(ImGui::GetContentRegionAvail().x, 32)))
            g_settings.selfDestruct = true;
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        return dirty;
    }
}
