#include "Tabs.h"
#include "../OverlayWidgets.h"
#include "../../Settings.h"
#include "imgui.h"
#include "imgui_internal.h"

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

            ImGuiStorage* storage = ImGui::GetStateStorage();
            const ImGuiID openId  = ImGui::GetID("##open");
            bool open = storage->GetBool(openId, false);

            const float btnW   = 24.0f;
            const float bindW  = 80.0f; 
            const float gap    = 6.0f;
            const float availX = ImGui::GetContentRegionAvail().x;

            const float nameW  = availX - bindW - (2.0f * btnW) - (3.0f * gap);

            // 1. Macro Name Input Field
            ImGui::SetNextItemWidth(nameW);
            if (ImGui::InputTextWithHint("##name", "item name (e.g. golden apple)", 
                                         g_settings.macros[i].name, 
                                         sizeof(g_settings.macros[i].name))) {
                dirty = true;
            }

            // 2. Inline Keybind Box
            ImGui::SameLine(0, gap);
            ImGui::SetNextItemWidth(bindW);
            dirty |= RowKeybind("##key", &g_settings.macros[i].key);

            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,        FromHex(Theme::Transparent));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, FromHex(Theme::ListBtnHovered));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  FromHex(Theme::ListBtnActive));

            // 3. Delete Button
            ImGui::SameLine(0, gap);
            if (ImGui::Button("##del", ImVec2(btnW, 24))) {
                toDelete = i;
            }
            {
                const ImVec2 bmin = ImGui::GetItemRectMin();
                const ImVec2 bmax = ImGui::GetItemRectMax();
                const ImVec2 ctr((bmin.x + bmax.x) * 0.5f, (bmin.y + bmax.y) * 0.5f);
                ImGui::GetWindowDrawList()->AddLine(
                    ImVec2(ctr.x - 5.0f, ctr.y), ImVec2(ctr.x + 5.0f, ctr.y),
                    ImGui::GetColorU32(ImGuiCol_Text), 2.0f);
            }

            // 4. Expander Chevron Button
            ImGui::SameLine(0, gap);
            if (ImGui::Button("##exp", ImVec2(btnW, 24))) {
                open = !open;
                storage->SetBool(openId, open);
            }
            {
                const ImVec2 bmin = ImGui::GetItemRectMin();
                const ImVec2 bmax = ImGui::GetItemRectMax();
                const ImVec2 ctr((bmin.x + bmax.x) * 0.5f, (bmin.y + bmax.y) * 0.5f);
                ImDrawList* dl   = ImGui::GetWindowDrawList();
                const ImU32  col = ImGui::GetColorU32(ImGuiCol_Text);
                
                // Set path flags for anti-aliased lines with rounded ends
                const float thickness = 2.5f; 
                
                if (open) {
                    // Downward Chevron
                    ImVec2 points[3] = {
                        ImVec2(ctr.x - 4.5f, ctr.y - 2.0f),
                        ImVec2(ctr.x,        ctr.y + 2.5f),
                        ImVec2(ctr.x + 4.5f, ctr.y - 2.0f)
                    };
                    dl->AddPolyline(points, 3, col, 0, thickness);
                } else {
                    // Rightward Chevron
                    ImVec2 points[3] = {
                        ImVec2(ctr.x - 2.0f, ctr.y - 4.5f),
                        ImVec2(ctr.x + 2.5f, ctr.y),
                        ImVec2(ctr.x - 2.0f, ctr.y + 4.5f)
                    };
                    dl->AddPolyline(points, 3, col, 0, thickness);
                }
            }

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();

            if (open) {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 6.0f));
                
                ImGui::Indent(12.0f);
                dirty |= RowSlider("Delay (ms)", &g_settings.macros[i].delay, 0, 5000, "%d ms");
                ImGui::Unindent(12.0f);
                
                ImGui::PopStyleVar();
            }

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
            ImGui::PushStyleColor(ImGuiCol_Button,        FromHex(Theme::AddBtn));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, FromHex(Theme::AddBtnHovered));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  FromHex(Theme::AddBtnActive));
            if (ImGui::Button("+ Add Macro", ImVec2(ImGui::GetContentRegionAvail().x, 32))) {
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