#include "Tabs.h"
#include "../OverlayWidgets.h"
#include "../../Settings.h"
#include "imgui.h"
#include <cctype>
#include <mutex>
#include <string>
#include <vector>

namespace OverlayTabs
{
    bool RenderFriends()
    {
        using namespace OverlayWidgets;
        bool dirty = false;

        ImGui::PopStyleVar();

        dirty |= RowCheckbox("Teams by Colour", &g_settings.teamsByColor);

        ImGui::Dummy(ImVec2(0, 6));

        dirty |= RowKeybind("Bind", &g_settings.friendKey, true);

        ImGui::Dummy(ImVec2(0, 6));

        static char addBuf[32] = {};
        ImGui::PushItemWidth(-80.0f);
        bool submitted = ImGui::InputTextWithHint(
            "##friendadd", "username (Enter to add)",
            addBuf, sizeof(addBuf),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 6);
        bool clicked = ImGui::Button("Add", ImVec2(74.0f, 0));

        if (submitted || clicked) {
            std::string name = addBuf;
            while (!name.empty() && std::isspace((unsigned char)name.front()))
                name.erase(name.begin());
            while (!name.empty() && std::isspace((unsigned char)name.back()))
                name.pop_back();
            for (char& c : name) c = (char)std::tolower((unsigned char)c);

            if (!name.empty()) {
                std::lock_guard<std::mutex> lk(g_settings.friendsMutex);
                bool exists = false;
                for (const auto& f : g_settings.friends)
                    if (f == name) { exists = true; break; }
                if (!exists) {
                    g_settings.friends.push_back(std::move(name));
                    dirty = true;
                }
            }
            addBuf[0] = '\0';
        }

        ImGui::Dummy(ImVec2(0, 6));

        std::vector<std::string> snap;
        {
            std::lock_guard<std::mutex> lk(g_settings.friendsMutex);
            snap = g_settings.friends;
        }

        if (!snap.empty()) {
            int toDelete = -1;
            for (int i = 0; i < (int)snap.size(); ++i) {
                ImGui::PushID(i);

                const float delW   = 24.0f;
                const float availX = ImGui::GetContentRegionAvail().x;

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(snap[i].c_str());

                ImGui::SameLine(availX - delW);
                ImGui::PushStyleColor(ImGuiCol_Button,        FromHex(0x2a1212, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, FromHex(0x9b1c1c, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  FromHex(0x7a1414, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text,          FromHex(0xffffff));
                if (ImGui::Button("x", ImVec2(delW, 24)))
                    toDelete = i;
                ImGui::PopStyleColor(4);

                ImGui::PopID();
            }
            if (toDelete >= 0) {
                std::lock_guard<std::mutex> lk(g_settings.friendsMutex);
                for (auto it = g_settings.friends.begin();
                     it != g_settings.friends.end(); ++it) {
                    if (*it == snap[toDelete]) {
                        g_settings.friends.erase(it);
                        dirty = true;
                        break;
                    }
                }
            }
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        return dirty;
    }
}
