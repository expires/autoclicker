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
        ImGui::PopStyleColor();

        return dirty;
    }
}
