#include "Tabs.h"
#include "../OverlayWidgets.h"
#include "../../Settings.h"
#include "imgui.h"

namespace OverlayTabs
{
    bool RenderMacros()
    {
        using namespace OverlayWidgets;
        bool dirty = false;

        ImGui::PopStyleVar();

        int toDelete = -1;

        for (int i = 0; i < g_settings.macroCount; ++i) {
            ImGui::PushID(i);

            const char* hdr = g_settings.macros[i].name[0]
                ? g_settings.macros[i].name
                : "Unnamed Macro";
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
            ImGui::TextUnformatted(hdr);
            ImGui::PopFont();

            const float delW   = 24.0f;
            const float availX = ImGui::GetContentRegionAvail().x;
            ImGui::SameLine(availX - delW);
            ImGui::PushStyleColor(ImGuiCol_Button,        FromHex(0x161d2e, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, FromHex(0x9b1c1c, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  FromHex(0x7a1414, 0.8f));
            if (ImGui::Button("x##del", ImVec2(delW, 24)))
                toDelete = i;
            ImGui::PopStyleColor(3);

            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputTextWithHint("##name",
                    "item name (e.g. golden apple)",
                    g_settings.macros[i].name,
                    sizeof(g_settings.macros[i].name)))
                dirty = true;

            ImGui::SetNextItemWidth(140.0f);
            if (ImGui::InputInt("Delay (ms)",
                    &g_settings.macros[i].delay, 10, 100,
                    ImGuiInputTextFlags_CharsDecimal)) {
                if (g_settings.macros[i].delay < 0)    g_settings.macros[i].delay = 0;
                if (g_settings.macros[i].delay > 5000) g_settings.macros[i].delay = 5000;
                dirty = true;
            }

            dirty |= RowKeybind("Key", &g_settings.macros[i].key);

            ImGui::Dummy(ImVec2(0, 6));
            ImGui::PopID();
        }

        if (toDelete >= 0 && toDelete < g_settings.macroCount) {
            for (int j = toDelete; j < g_settings.macroCount - 1; ++j)
                g_settings.macros[j] = g_settings.macros[j + 1];
            g_settings.macros[g_settings.macroCount - 1] = Macro{};
            g_settings.macroCount--;
            dirty = true;
        }

        if (g_settings.macroCount < Settings::MAX_MACROS) {
            ImGui::PushStyleColor(ImGuiCol_Button,        FromHex(0x5865f2, 0.18f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, FromHex(0x5865f2, 0.35f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  FromHex(0x5865f2, 0.55f));
            if (ImGui::Button("+ Add Macro",
                    ImVec2(ImGui::GetContentRegionAvail().x, 32))) {
                g_settings.macros[g_settings.macroCount] = Macro{};
                g_settings.macroCount++;
                dirty = true;
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        return dirty;
    }
}
