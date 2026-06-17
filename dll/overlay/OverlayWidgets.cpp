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

    ImVec4 FromHex(const Theme::Col& c)
    {
        return FromHex(c.hex, c.a);
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

        const float  w    = GetContentRegionAvail().x;
        const float  rowH = 30.0f;
        const ImVec2 pos  = window->DC.CursorPos;
        const ImRect bb(pos, ImVec2(pos.x + w, pos.y + rowH));

        ImGuiID id = window->GetID(label);
        ItemSize(bb);
        if (!ItemAdd(bb, id)) return false;

        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held);
        if (pressed) { *v = !*v; MarkItemEdited(id); }

        ImGuiStorage* storage = &window->StateStorage;
        float anim = storage->GetFloat(id, *v ? 1.0f : 0.0f);
        anim = ImLerp(anim, *v ? 1.0f : 0.0f, 0.20f);
        storage->SetFloat(id, anim);

        const float  pillW = 42.0f;
        const float  pillH = 22.0f;
        const ImVec2 pMin(bb.Max.x - pillW, bb.Min.y + (rowH - pillH) * 0.5f);
        const ImVec2 pMax(bb.Max.x, pMin.y + pillH);
        const float  rad = pillH * 0.5f;

        ImDrawList* dl = window->DrawList;

        const ImU32 trackOff = GetColorU32(ImGuiCol_FrameBg);
        const ImU32 trackOn  = ColorConvertFloat4ToU32(FromHex(Theme::AccentTrack));
        const ImU32 track    = LerpU32(trackOff, trackOn, anim);
        dl->AddRectFilled(pMin, pMax, track, rad);
        dl->AddRect(pMin, pMax, GetColorU32(ImGuiCol_Border), rad, 0, 1.0f);
        dl->AddLine(ImVec2(pMin.x + rad, pMin.y + 1.0f),
                    ImVec2(pMax.x - rad, pMin.y + 1.0f),
                    IM_COL32(255, 255, 255, 26), 1.0f);

        const float knobX = pMin.x + rad + anim * (pillW - pillH);
        const float knobY = pMin.y + rad;
        const float knobR = rad - 3.0f;
        dl->AddCircleFilled(ImVec2(knobX, knobY + 1.0f), knobR, IM_COL32(0, 0, 0, 60));
        dl->AddCircleFilled(ImVec2(knobX, knobY), knobR, IM_COL32(255, 255, 255, 236));

        ImVec2 labelSz = CalcTextSize(label, nullptr, true);
        RenderText(ImVec2(bb.Min.x, bb.GetCenter().y - labelSz.y * 0.5f), label);

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

        const float pr = 6.0f;
        const ImU32 pillBg = listening
            ? ColorConvertFloat4ToU32(FromHex(Theme::KeybindListening))
            : (hovered ? GetColorU32(ImGuiCol_FrameBgHovered)
                       : GetColorU32(ImGuiCol_FrameBg));
        ImDrawList* dl = window->DrawList;
        dl->AddRectFilled(pill.Min, pill.Max, pillBg, pr);
        dl->AddRect(pill.Min, pill.Max, GetColorU32(ImGuiCol_Border), pr, 0, 1.0f);
        dl->AddLine(ImVec2(pill.Min.x + pr, pill.Min.y + 1.0f),
                    ImVec2(pill.Max.x - pr, pill.Min.y + 1.0f),
                    IM_COL32(255, 255, 255, 22), 1.0f);

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
        const float  knobPad = 11.0f;
        const ImRect frame(ImVec2(total.Min.x + knobPad, total.Min.y + labelSz.y + 12.0f),
                           ImVec2(total.Max.x - knobPad, total.Max.y - 13.0f));

        const ImRect hitFrame(ImVec2(total.Min.x + knobPad, total.Min.y + labelSz.y + 4.0f),
                              ImVec2(total.Max.x - knobPad, total.Max.y - 4.0f));

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

        const float fillW  = anim * frame.GetWidth();
        const float trackR = frame.GetHeight() * 0.5f;
        const ImU32 accent = ColorConvertFloat4ToU32(FromHex(Theme::Accent));

        ImDrawList* dl = window->DrawList;
        dl->AddRectFilled(frame.Min, frame.Max, GetColorU32(ImGuiCol_FrameBg), trackR);
        if (fillW > trackR)
            dl->AddRectFilled(frame.Min,
                ImVec2(frame.Min.x + fillW, frame.Max.y), accent, trackR);
        dl->AddLine(ImVec2(frame.Min.x + trackR, frame.Min.y + 0.5f),
                    ImVec2(frame.Max.x - trackR, frame.Min.y + 0.5f),
                    IM_COL32(255, 255, 255, 20), 1.0f);

        const ImVec2 knob(frame.Min.x + fillW, frame.GetCenter().y);
        dl->AddCircleFilled(knob, 9.5f, ColorConvertFloat4ToU32(FromHex(Theme::AccentGlow)));
        dl->AddCircleFilled(ImVec2(knob.x, knob.y + 1.0f), 6.5f, IM_COL32(0, 0, 0, 55));
        dl->AddCircleFilled(knob, 6.5f, GetColorU32(ImGuiCol_SliderGrab));

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
        const float  mx = 10.0f;
        const float  my = 1.0f;
        const ImVec2 rMin(bb.Min.x + mx, bb.Min.y + my);
        const ImVec2 rMax(bb.Max.x - mx, bb.Max.y - my);
        const float  rr = 7.0f;

        if (selected) {
            dl->AddRectFilled(rMin, rMax, ColorConvertFloat4ToU32(FromHex(Theme::TabSelectedFill)), rr);
        } else if (hovered) {
            dl->AddRectFilled(rMin, rMax, ColorConvertFloat4ToU32(FromHex(Theme::TabHover)), rr);
        }

        const ImU32 textCol = ColorConvertFloat4ToU32(
            selected ? FromHex(Theme::TabTextActive) : FromHex(Theme::TabTextInactive));

        PushStyleColor(ImGuiCol_Text, textCol);

        ImVec2 labelSz = CalcTextSize(label);
        RenderText(ImVec2(bb.Min.x + 20.0f,
                          bb.GetCenter().y - labelSz.y * 0.5f), label);

        PopStyleColor();
        return pressed;
    }
}
