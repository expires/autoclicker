#define IMGUI_DEFINE_MATH_OPERATORS
#include "OverlayWidgets.h"
#include <Windows.h>
#include <cstdio>
#include "imgui.h"
#include "imgui_internal.h"
#include "../Settings.h"

namespace OverlayWidgets
{
    static ImGuiID s_kbActiveId      = 0;
    static bool    s_kbExcluded[256] = {};
    static bool    s_keybindListening = false;

    bool IsKeybindListening()    { return s_keybindListening; }
    void ResetKeybindListening() { s_keybindListening = false; }

    ImVec4 FromHex(uint32_t hex, float alpha)
    {
        return ImVec4(
            ((hex >> 16) & 0xFF) / 255.0f,
            ((hex >>  8) & 0xFF) / 255.0f,
            ( hex        & 0xFF) / 255.0f,
            alpha);
    }

    static ImU32 LerpU32(ImU32 a, ImU32 b, float t)
    {
        if (t <= 0.0f) return a;
        if (t >= 1.0f) return b;
        int ar = (a >> IM_COL32_R_SHIFT) & 0xFF;
        int ag = (a >> IM_COL32_G_SHIFT) & 0xFF;
        int ab = (a >> IM_COL32_B_SHIFT) & 0xFF;
        int aa = (a >> IM_COL32_A_SHIFT) & 0xFF;
        int br = (b >> IM_COL32_R_SHIFT) & 0xFF;
        int bg = (b >> IM_COL32_G_SHIFT) & 0xFF;
        int bb = (b >> IM_COL32_B_SHIFT) & 0xFF;
        int ba = (b >> IM_COL32_A_SHIFT) & 0xFF;
        int r = ar + (int)((br - ar) * t);
        int g = ag + (int)((bg - ag) * t);
        int B = ab + (int)((bb - ab) * t);
        int A = aa + (int)((ba - aa) * t);
        return IM_COL32(r, g, B, A);
    }

    bool RowCheckbox(const char* label, bool* v)
    {
        using namespace ImGui;
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        const float w         = GetContentRegionAvail().x;
        const float square_sz = 20.0f;
        const ImVec2 pos      = window->DC.CursorPos;
        const ImRect bb(pos, ImVec2(pos.x + w, pos.y + square_sz + 10.0f));

        ImGuiID id = window->GetID(label);
        ItemSize(bb);
        if (!ItemAdd(bb, id)) return false;

        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held);
        if (pressed) { *v = !*v; MarkItemEdited(id); }

        ImGuiStorage* storage = &window->StateStorage;
        float anim = storage->GetFloat(id, *v ? 1.0f : 0.0f);
        anim = ImLerp(anim, *v ? 1.0f : 0.0f, 0.18f);
        storage->SetFloat(id, anim);

        const ImU32 colOff = GetColorU32(ImGuiCol_FrameBg);
        const ImU32 colOn  = ColorConvertFloat4ToU32(FromHex(0x5865f2));
        const ImU32 fill   = LerpU32(colOff, colOn, anim);

        ImVec2 sqMin(bb.Max.x - square_sz, bb.Min.y);
        ImVec2 sqMax(bb.Max.x,             bb.Min.y + square_sz);
        window->DrawList->AddRectFilled(sqMin, sqMax, fill, 2.0f);

        if (anim > 0.01f) {
            ImU32 cm = IM_COL32(255, 255, 255, (int)(255.0f * anim));
            RenderCheckMark(window->DrawList,
                ImVec2(sqMax.x - square_sz * 0.5f - 3.5f, bb.Min.y + square_sz * 0.5f - 3.5f),
                cm, 7.0f);
        }

        ImVec2 labelSz = CalcTextSize(label, nullptr, true);
        RenderText(ImVec2(bb.Min.x, bb.Max.y - labelSz.y - 13.0f), label);

        return pressed;
    }

    static const char* GetKeyName(int vk)
    {
        if (vk == 0) return "none";
        static char buf[32];
        UINT scan = MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_VSC);
        LONG lp   = (LONG)(scan << 16);
        switch (vk) {
            case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END:
            case VK_PRIOR:  case VK_NEXT:
            case VK_UP:     case VK_DOWN:  case VK_LEFT: case VK_RIGHT:
            case VK_DIVIDE: case VK_NUMLOCK:
                lp |= (1L << 24); break;
            default: break;
        }
        if (GetKeyNameTextA(lp, buf, sizeof(buf)) > 0) return buf;
        snprintf(buf, sizeof(buf), "VK_%02X", vk);
        return buf;
    }

    bool RowKeybind(const char* label, int* vk, bool allowMouse)
    {
        using namespace ImGui;
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        const float  w   = GetContentRegionAvail().x;
        const float  h   = 30.0f;
        const ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, ImVec2(pos.x + w, pos.y + h));

        ImGuiID id = window->GetID(label);
        ItemSize(bb);
        if (!ItemAdd(bb, id)) return false;

        const float pillW = 120.0f;
        const ImRect pill(ImVec2(bb.Max.x - pillW, bb.Min.y + 4.0f),
                          ImVec2(bb.Max.x,         bb.Max.y - 4.0f));

        bool hovered, held;
        bool pressed = ButtonBehavior(pill, id, &hovered, &held);

        bool changed = false;

        auto isFilteredVk = [allowMouse](int k) {
            if (k == VK_LBUTTON || k == VK_RBUTTON) return true;
            if (!allowMouse &&
                (k == VK_MBUTTON || k == VK_XBUTTON1 || k == VK_XBUTTON2))
                return true;
            if (k == VK_SHIFT || k == VK_CONTROL || k == VK_MENU) return true;
            return false;
        };

        if (pressed) {
            if (s_kbActiveId == id) {
                s_kbActiveId = 0;
            } else {
                s_kbActiveId = id;
                for (int k = 0; k < 256; ++k)
                    s_kbExcluded[k] = (GetAsyncKeyState(k) & 0x8000) != 0;
            }
        }

        if (IsMouseHoveringRect(pill.Min, pill.Max) &&
            IsMouseClicked(ImGuiMouseButton_Right)) {
            if (*vk != 0) { *vk = 0; changed = true; }
            if (s_kbActiveId == id) s_kbActiveId = 0;
        }

        const bool listening = (s_kbActiveId == id);

        if (listening) {
            s_keybindListening = true;

            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                s_kbActiveId = 0;
            }
            else {
                for (int k = 0; k < 256; ++k) {
                    if (s_kbExcluded[k] && !(GetAsyncKeyState(k) & 0x8000))
                        s_kbExcluded[k] = false;
                }

                const int menuVk = (g_settings.menuKey > 0 && g_settings.menuKey <= 0xFE)
                    ? g_settings.menuKey : VK_RSHIFT;
                const int loopStart = allowMouse ? 0x01 : 0x07;
                for (int k = loopStart; k <= 0xFE; ++k) {
                    if (isFilteredVk(k))  continue;
                    if (k == VK_ESCAPE)   continue;
                    if (k == menuVk)      continue;
                    if (s_kbExcluded[k])  continue;
                    if (!(GetAsyncKeyState(k) & 0x8000)) continue;
                    *vk     = k;
                    changed = true;
                    s_kbActiveId = 0;
                    break;
                }
            }
        }

        ImVec2 labelSz = CalcTextSize(label, nullptr, true);
        PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_Text));
        RenderText(ImVec2(bb.Min.x, bb.GetCenter().y - labelSz.y * 0.5f), label);
        PopStyleColor();

        const ImU32 pillBg = listening
            ? ColorConvertFloat4ToU32(FromHex(0x5865f2))
            : (hovered ? GetColorU32(ImGuiCol_FrameBgHovered)
                       : GetColorU32(ImGuiCol_FrameBg));
        window->DrawList->AddRectFilled(pill.Min, pill.Max, pillBg, 4.0f);

        const char* pillText = listening
            ? "press a key..."
            : (*vk ? GetKeyName(*vk) : "none");
        ImVec2 textSz = CalcTextSize(pillText);
        RenderText(ImVec2(pill.GetCenter().x - textSz.x * 0.5f,
                          pill.GetCenter().y - textSz.y * 0.5f),
                   pillText);

        return changed;
    }

    bool RowSlider(const char* label, int* v, int v_min, int v_max, const char* fmt)
    {
        using namespace ImGui;
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g       = *GImGui;
        const float w         = GetContentRegionAvail().x;
        const ImVec2 labelSz  = CalcTextSize(label, nullptr, true);
        const ImRect total(window->DC.CursorPos,
                           ImVec2(window->DC.CursorPos.x + w,
                                  window->DC.CursorPos.y + labelSz.y + 30.0f));
        const ImRect frame(ImVec2(total.Min.x, total.Min.y + labelSz.y + 12.0f),
                           ImVec2(total.Max.x, total.Max.y - 13.0f));

        const ImRect hitFrame(ImVec2(total.Min.x, total.Min.y + labelSz.y + 4.0f),
                              ImVec2(total.Max.x, total.Max.y - 4.0f));

        ImGuiID id = window->GetID(label);
        ItemSize(total, g.Style.FramePadding.y);
        if (!ItemAdd(total, id, &hitFrame)) return false;

        bool hovered, held;
        ButtonBehavior(hitFrame, id, &hovered, &held);

        ImRect grab_bb;
        bool changed = SliderBehavior(hitFrame, id, ImGuiDataType_S32, v,
                                      &v_min, &v_max, fmt, ImGuiSliderFlags_None, &grab_bb);
        if (changed) MarkItemEdited(id);

        const float range = (float)(v_max - v_min);
        float t = range > 0.0f ? (float)(*v - v_min) / range : 0.0f;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        ImGuiStorage* storage = &window->StateStorage;
        float anim = storage->GetFloat(id, t);
        anim = ImLerp(anim, t, 0.20f);
        storage->SetFloat(id, anim);

        char buf[32];
        snprintf(buf, sizeof(buf), fmt, *v);

        RenderText(total.Min, label);
        ImVec2 valSz = CalcTextSize(buf);
        RenderText(ImVec2(total.Max.x - valSz.x, total.Min.y), buf);

        const float fillW = anim * frame.GetWidth();
        const ImU32 accent = ColorConvertFloat4ToU32(FromHex(0x5865f2));

        window->DrawList->AddRectFilled(frame.Min, frame.Max,
            GetColorU32(ImGuiCol_FrameBg), 2.0f);
        window->DrawList->AddRectFilled(frame.Min,
            ImVec2(frame.Min.x + fillW, frame.Max.y), accent, 2.0f);
        window->DrawList->AddCircleFilled(
            ImVec2(frame.Min.x + fillW, frame.GetCenter().y), 8.0f,
            GetColorU32(ImGuiCol_SliderGrab));

        return changed;
    }

    bool RowInputInt(const char* label, int* v, int v_min, int v_max, int step, int fastStep)
    {
        using namespace ImGui;
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        const float rowH    = 36.0f;
        const float w       = GetContentRegionAvail().x;
        const float inputW  = 160.0f;
        const ImVec2 origin = window->DC.CursorPos;

        const ImRect rowBB(origin, ImVec2(origin.x + w, origin.y + rowH));
        ItemSize(rowBB);

        const ImVec2 labelSz = CalcTextSize(label);
        RenderText(ImVec2(origin.x, origin.y + (rowH - labelSz.y) * 0.5f), label);

        ImGuiContext& g = *GImGui;
        const float btn = GetFrameHeight();
        const float gap = g.Style.ItemInnerSpacing.x;
        const float fieldW = inputW - 2.0f * (btn + gap);
        SetCursorScreenPos(ImVec2(origin.x + w - inputW,
                                  origin.y + (rowH - GetFrameHeight()) * 0.5f));
        SetNextItemWidth(fieldW);

        char hidden[64];
        snprintf(hidden, sizeof(hidden), "##%s", label);
        bool changed = InputInt(hidden, v, step, fastStep,
                                ImGuiInputTextFlags_CharsDecimal);
        if (changed) {
            if (*v < v_min) *v = v_min;
            if (*v > v_max) *v = v_max;
        }

        return changed;
    }

    bool RowInputIntPair(const char* labelA, int* vA, int minA, int maxA, int stepA, int fastA,
                         const char* labelB, int* vB, int minB, int maxB, int stepB, int fastB)
    {
        using namespace ImGui;
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        const float rowH    = 36.0f;
        const float w       = GetContentRegionAvail().x;
        const float gutter  = 6.0f;
        const float halfW   = (w - gutter) * 0.5f;
        const float inputW  = 110.0f;
        const ImVec2 origin = window->DC.CursorPos;

        const ImRect rowBB(origin, ImVec2(origin.x + w, origin.y + rowH));
        ItemSize(rowBB);

        ImGuiContext& g = *GImGui;
        const float btn = GetFrameHeight();
        const float gap = g.Style.ItemInnerSpacing.x;
        const float fieldW = inputW - 2.0f * (btn + gap);

        auto drawHalf = [&](float originX, const char* label, int* v, int vMin, int vMax,
                            int step, int fast) -> bool {
            const ImVec2 labelSz = CalcTextSize(label);
            RenderText(ImVec2(originX, origin.y + (rowH - labelSz.y) * 0.5f), label);

            SetCursorScreenPos(ImVec2(originX + halfW - inputW,
                                      origin.y + (rowH - GetFrameHeight()) * 0.5f));
            SetNextItemWidth(fieldW);

            char hidden[64];
            snprintf(hidden, sizeof(hidden), "##%s", label);
            bool changed = InputInt(hidden, v, step, fast,
                                    ImGuiInputTextFlags_CharsDecimal);
            if (changed) {
                if (*v < vMin) *v = vMin;
                if (*v > vMax) *v = vMax;
            }
            return changed;
        };

        bool changedA = drawHalf(origin.x,                  labelA, vA, minA, maxA, stepA, fastA);
        bool changedB = drawHalf(origin.x + halfW + gutter, labelB, vB, minB, maxB, stepB, fastB);

        return changedA || changedB;
    }

    bool SidebarTab(const char* label, bool selected)
    {
        using namespace ImGui;
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        ImVec2 p = window->DC.CursorPos;
        ImVec2 size(window->Size.x, 32.0f);
        ImRect bb(p, ImVec2(p.x + size.x, p.y + size.y));

        ImGuiID id = window->GetID(label);
        ItemSize(bb);
        if (!ItemAdd(bb, id)) return false;

        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held);

        ImDrawList* dl = window->DrawList;
        if (hovered && !selected)
            dl->AddRectFilled(bb.Min, bb.Max,
                ColorConvertFloat4ToU32(FromHex(0x161d2e, 0.5f)));

        static float line_pos = 0.f;
        if (selected) line_pos = ImLerp(line_pos, bb.Min.y - window->Pos.y, 0.20f);
        dl->AddRectFilled(
            ImVec2(bb.Max.x - 2.0f, window->Pos.y + line_pos),
            ImVec2(bb.Max.x,        window->Pos.y + line_pos + size.y),
            ColorConvertFloat4ToU32(FromHex(0x5865f2)),
            2.0f, ImDrawFlags_RoundCornersLeft);

        const ImU32 textCol = ColorConvertFloat4ToU32(
            selected ? FromHex(0xffffff) : FromHex(0x707a8c));

        PushStyleColor(ImGuiCol_Text, textCol);

        ImVec2 labelSz = CalcTextSize(label);
        RenderText(ImVec2(bb.Min.x + 20.0f,
                          bb.GetCenter().y - labelSz.y * 0.5f), label);

        PopStyleColor();
        return pressed;
    }
}
