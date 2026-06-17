#include "Tabs.h"
#include "../OverlayWidgets.h"
#include "../../config/Settings.h"
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

            const float btnW   = Theme::M::ListBtnW;
            const float bindW  = Theme::M::ListBindW;
            const float gap    = Theme::M::ListGap;
            const float availX = ImGui::GetContentRegionAvail().x;

            const float nameW  = availX - bindW - (2.0f * btnW) - (3.0f * gap);

            ImGui::SetNextItemWidth(nameW);
            if (ImGui::InputTextWithHint("##name", "item name (e.g. golden apple)", 
                                         g_settings.macros[i].name, 
                                         sizeof(g_settings.macros[i].name))) {
                dirty = true;
            }

            ImGui::SameLine(0, gap);
            dirty |= RowKeybind("##key", &g_settings.macros[i].key, bindW);

            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,        FromHex(Theme::Transparent));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, FromHex(Theme::ListBtnHovered));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  FromHex(Theme::ListBtnActive));

            ImGui::SameLine(0, gap);
            if (ImGui::Button("##del", ImVec2(btnW, btnW))) {
                toDelete = i;
            }
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            {
                const ImVec2 bmin = ImGui::GetItemRectMin();
                const ImVec2 bmax = ImGui::GetItemRectMax();
                const ImVec2 ctr((bmin.x + bmax.x) * 0.5f, (bmin.y + bmax.y) * 0.5f);
                ImGui::GetWindowDrawList()->AddLine(
                    ImVec2(ctr.x - Theme::px(5.0f), ctr.y), ImVec2(ctr.x + Theme::px(5.0f), ctr.y),
                    ImGui::GetColorU32(ImGuiCol_Text), Theme::px(2.0f));
            }

            ImGui::SameLine(0, gap);
            if (ImGui::Button("##exp", ImVec2(btnW, btnW))) {
                open = !open;
                storage->SetBool(openId, open);
            }
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            {
                const ImVec2 bmin = ImGui::GetItemRectMin();
                const ImVec2 bmax = ImGui::GetItemRectMax();
                const ImVec2 ctr((bmin.x + bmax.x) * 0.5f, (bmin.y + bmax.y) * 0.5f);
                ImDrawList* dl   = ImGui::GetWindowDrawList();
                const ImU32  col = ImGui::GetColorU32(ImGuiCol_Text);
                
                const float thickness = Theme::px(2.5f);

                if (open) {
                    ImVec2 points[3] = {
                        ImVec2(ctr.x - Theme::px(4.5f), ctr.y - Theme::px(2.0f)),
                        ImVec2(ctr.x,                   ctr.y + Theme::px(2.5f)),
                        ImVec2(ctr.x + Theme::px(4.5f), ctr.y - Theme::px(2.0f))
                    };
                    dl->AddPolyline(points, 3, col, 0, thickness);
                } else {
                    ImVec2 points[3] = {
                        ImVec2(ctr.x - Theme::px(2.0f), ctr.y - Theme::px(4.5f)),
                        ImVec2(ctr.x + Theme::px(2.5f), ctr.y),
                        ImVec2(ctr.x - Theme::px(2.0f), ctr.y + Theme::px(4.5f))
                    };
                    dl->AddPolyline(points, 3, col, 0, thickness);
                }
            }

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();

            if (open) {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, Theme::M::RowSpacing));

                ImGui::Indent(Theme::M::MacroIndent);
                dirty |= RowSlider("Delay (ms)", &g_settings.macros[i].delay, 0, 5000, "%d ms");
                ImGui::Unindent(Theme::M::MacroIndent);
                
                ImGui::PopStyleVar();
            }

            ImGui::Dummy(ImVec2(0, Theme::M::RowSpacing));
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
            if (ImGui::Button("+ Add Macro", ImVec2(ImGui::GetContentRegionAvail().x, Theme::M::AddBtnH))) {
                g_settings.macros[g_settings.macroCount] = Macro{};
                g_settings.macroCount++;
                dirty = true;
            }
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::PopStyleColor(3);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        return dirty;
    }
}