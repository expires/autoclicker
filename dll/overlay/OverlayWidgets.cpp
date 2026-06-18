#define IMGUI_DEFINE_MATH_OPERATORS
#include "OverlayWidgets.h"
#include <Windows.h>
#include <cstdio>
#include "imgui.h"
#include "imgui_internal.h"
#include "../config/Settings.h"

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

    bool KeybindSquare(const char* id, int* vk, float size, bool allowMouse)
    {
        using namespace ImGui;
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        const float s = (size > 0.0f) ? size : Theme::M::CheckRowH;
        const ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, ImVec2(pos.x + s, pos.y + s));

        ImGuiID gid = window->GetID(id);
        ItemSize(bb);
        if (!ItemAdd(bb, gid)) return false;

        bool hovered, held;
        bool pressed = ButtonBehavior(bb, gid, &hovered, &held);

        if (pressed) {
            if (s_kbActiveId == gid) {
                s_kbActiveId = 0;
            } else {
                s_kbActiveId = gid;
                for (int k = 0; k < 256; ++k)
                    s_kbExcluded[k] = (GetAsyncKeyState(k) & 0x8000) != 0;
            }
        }

        if (hovered && IsMouseClicked(ImGuiMouseButton_Right)) {
            if (*vk != 0) { *vk = 0; MarkItemEdited(gid); }
            if (s_kbActiveId == gid) s_kbActiveId = 0;
        }

        const bool listening = (s_kbActiveId == gid);
        bool changed = false;

        if (listening) {
            s_keybindListening = true;
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                s_kbActiveId = 0;
            } else {
                for (int k = 0; k < 256; ++k) {
                    if (s_kbExcluded[k] && !(GetAsyncKeyState(k) & 0x8000))
                        s_kbExcluded[k] = false;
                }
                const int menuVk = (g_settings.menuKey > 0 && g_settings.menuKey <= 0xFE) ? g_settings.menuKey : VK_RSHIFT;
                const int loopStart = allowMouse ? 0x01 : 0x07;
                for (int k = loopStart; k <= 0xFE; ++k) {
                    if (k == VK_SHIFT || k == VK_CONTROL || k == VK_MENU || k == VK_ESCAPE || k == menuVk) continue;
                    if (!allowMouse && (k == VK_LBUTTON || k == VK_RBUTTON)) continue;
                    if (s_kbExcluded[k]) continue;
                    if (!(GetAsyncKeyState(k) & 0x8000)) continue;
                    *vk = k;
                    changed = true;
                    MarkItemEdited(gid);
                    s_kbActiveId = 0;
                    break;
                }
            }
        }

        ImDrawList* dl = window->DrawList;
        const ImU32 bg = listening ? ColorConvertFloat4ToU32(FromHex(Theme::KeybindListening)) : (hovered ? GetColorU32(ImGuiCol_FrameBgHovered) : GetColorU32(ImGuiCol_FrameBg));
        const float r = Theme::M::KeybindRound;
        const float pad = Theme::px(4.0f);
        const ImRect inner(bb.Min + ImVec2(pad, pad), bb.Max - ImVec2(pad, pad));

        dl->AddRectFilled(inner.Min, inner.Max, bg, r);
        dl->AddRect(inner.Min, inner.Max, GetColorU32(ImGuiCol_Border), r, 0, 1.0f);
        
        const ImU32 accent = ColorConvertFloat4ToU32(FromHex(Theme::Accent));
        dl->AddRectFilled(ImVec2(inner.Min.x + 4, inner.Max.y - 3), ImVec2(inner.Max.x - 4, inner.Max.y - 1), accent, 1.0f);

        if (*vk == 0 && !listening) {
            const float dotR = Theme::px(1.5f);
            const ImVec2 c = inner.GetCenter();
            dl->AddCircleFilled(ImVec2(c.x - Theme::px(5.0f), c.y), dotR, GetColorU32(ImGuiCol_TextDisabled));
            dl->AddCircleFilled(ImVec2(c.x,                   c.y), dotR, GetColorU32(ImGuiCol_TextDisabled));
            dl->AddCircleFilled(ImVec2(c.x + Theme::px(5.0f), c.y), dotR, GetColorU32(ImGuiCol_TextDisabled));
        } else {
            const char* name = listening ? "?" : GetVirtualKeyName(*vk);
            ImVec2 sz = CalcTextSize(name);
            RenderText(inner.GetCenter() - sz * 0.5f, name);
        }

        if (hovered) SetMouseCursor(ImGuiMouseCursor_Hand);
        return changed;
    }

    bool ModuleHeader(const char* label, bool* v, int* vk)
    {
        using namespace ImGui;
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        const float  w    = GetContentRegionAvail().x;
        const float  rowH = Theme::M::CheckRowH;
        const ImVec2 pos  = window->DC.CursorPos;
        const ImRect bb(pos, ImVec2(pos.x + w, pos.y + rowH));

        ImGuiID id = window->GetID(label);
        ItemSize(bb);
        if (!ItemAdd(bb, id)) return false;

        const float  kbW    = rowH;
        const float  gap    = Theme::M::ListGap;
        const float  pillW  = Theme::M::PillW;
        const float  pillH  = Theme::M::PillH;
        const float  kbPad  = (rowH - pillH) * 0.5f;

        const ImRect kbBB(ImVec2(bb.Max.x - kbW, bb.Min.y), bb.Max);
        const ImRect pillBB(ImVec2(kbBB.Min.x - gap - pillW, bb.Min.y + kbPad), ImVec2(kbBB.Min.x - gap, bb.Min.y + kbPad + pillH));

        bool changed = false;
        bool hovered, held;
        if (ButtonBehavior(pillBB, id, &hovered, &held)) {
            *v = !*v;
            MarkItemEdited(id);
            changed = true;
        }

        PushID(id);
        SetCursorScreenPos(kbBB.Min);
        if (KeybindSquare("##kb", vk, kbW)) changed = true;
        PopID();

        ImDrawList* dl = window->DrawList;
        float anim = window->StateStorage.GetFloat(id, *v ? 1.0f : 0.0f);
        anim = ImLerp(anim, *v ? 1.0f : 0.0f, 0.20f);
        window->StateStorage.SetFloat(id, anim);

        const ImU32 trackOff = GetColorU32(ImGuiCol_FrameBg);
        const ImU32 trackOn  = ColorConvertFloat4ToU32(FromHex(Theme::AccentTrack));
        const ImU32 track    = LerpU32(trackOff, trackOn, anim);
        const float rad      = pillH * 0.5f;
        dl->AddRectFilled(pillBB.Min, pillBB.Max, track, rad);
        dl->AddRect(pillBB.Min, pillBB.Max, GetColorU32(ImGuiCol_Border), rad, 0, 1.0f);
        const float knobX = pillBB.Min.x + rad + anim * (pillW - pillH);
        const float knobY = pillBB.Min.y + rad;
        const float knobR = rad - Theme::M::KnobInset;
        dl->AddCircleFilled(ImVec2(knobX, knobY), knobR, IM_COL32(255, 255, 255, 236));

        ImVec2 labelSz = CalcTextSize(label, nullptr, true);
        RenderText(ImVec2(bb.Min.x, bb.GetCenter().y - labelSz.y * 0.5f), label);

        if (hovered) SetMouseCursor(ImGuiMouseCursor_Hand);
        SetCursorScreenPos(ImVec2(bb.Min.x, bb.Max.y));
        return changed;
    }

    static const char* GetVirtualKeyName(int vk)
    {
        if (vk == 0) return "NONE";
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

    bool RowKeybind(const char* label, int* vk, float customWidth, bool allowMouse)
    {
        using namespace ImGui;
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        const bool inlineMode = (label[0] == '#' && label[1] == '#');
        const float w = (inlineMode && customWidth > 0.0f) ? customWidth : GetContentRegionAvail().x;
        const float h = Theme::M::CheckRowH;
        const ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, ImVec2(pos.x + w, pos.y + h));

        if (!inlineMode) {
            ItemSize(bb);
            if (!ItemAdd(bb, window->GetID(label))) return false;
            ImVec2 labelSz = CalcTextSize(label, nullptr, true);
            RenderText(ImVec2(bb.Min.x, bb.GetCenter().y - labelSz.y * 0.5f), label);
        }

        const float kbW = h;
        SetCursorScreenPos(ImVec2(bb.Max.x - kbW, bb.Min.y));
        bool changed = KeybindSquare(label, vk, kbW, allowMouse);
        
        if (!inlineMode) SetCursorScreenPos(ImVec2(bb.Min.x, bb.Max.y));
        return changed;
    }

    bool RowKeybind(const char* label, int* vk, bool allowMouse)
    {
        return RowKeybind(label, vk, 0.0f, allowMouse);
    }

    bool RowSlider(const char* label, int* v, int v_min, int v_max, const char* fmt)
    {
        using namespace ImGui;
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g       = *GImGui;
        const bool inlineMode = (label[0] == '#' && label[1] == '#');
        const float w         = GetContentRegionAvail().x;
        
        const char* cleanLabel = inlineMode ? "Delay (ms)" : label;
        const ImVec2 labelSz  = CalcTextSize(cleanLabel, nullptr, true);
        
        const float heightOffset = labelSz.y + Theme::M::SliderLabelGap;
        const ImRect total(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + w, window->DC.CursorPos.y + heightOffset + Theme::M::SliderH));

        const ImRect frame(ImVec2(total.Min.x, total.Min.y + heightOffset + Theme::M::SliderTrackTop),
                           ImVec2(total.Max.x, total.Max.y - Theme::M::SliderTrackBot));

        const ImRect hitFrame(ImVec2(total.Min.x, total.Min.y + heightOffset),
                              ImVec2(total.Max.x, total.Max.y));

        ImGuiID id = window->GetID(label);
        ItemSize(total, g.Style.FramePadding.y);
        if (!ItemAdd(total, id, &hitFrame)) return false;

        bool hovered, held;
        ButtonBehavior(hitFrame, id, &hovered, &held);
        if (hovered) SetMouseCursor(ImGuiMouseCursor_Hand);

        ImRect grab_bb;
        bool changed = SliderBehavior(hitFrame, id, ImGuiDataType_S32, v, &v_min, &v_max, fmt, ImGuiSliderFlags_None, &grab_bb);
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

        RenderText(total.Min, cleanLabel);
        ImVec2 valSz = CalcTextSize(buf);
        RenderText(ImVec2(total.Max.x - valSz.x, total.Min.y), buf);

        const float fillW  = anim * frame.GetWidth();
        const float trackR = frame.GetHeight() * 0.5f;
        const ImU32 accent = ColorConvertFloat4ToU32(FromHex(Theme::Accent));

        ImDrawList* dl = window->DrawList;
        dl->AddRectFilled(frame.Min, frame.Max, GetColorU32(ImGuiCol_FrameBg), trackR);
        if (fillW > trackR)
            dl->AddRectFilled(frame.Min, ImVec2(frame.Min.x + fillW, frame.Max.y), accent, trackR);
        dl->AddLine(ImVec2(frame.Min.x + trackR, frame.Min.y + 0.5f),
                    ImVec2(frame.Max.x - trackR, frame.Min.y + 0.5f),
                    IM_COL32(255, 255, 255, 20), 1.0f);

        const float grabRadiusPadding = Theme::M::SliderGrabPad;
        const float knobX = ImLerp(frame.Min.x + grabRadiusPadding, frame.Max.x - grabRadiusPadding, anim);
        const ImVec2 knob(knobX, frame.GetCenter().y);

        dl->AddCircleFilled(knob, Theme::M::SliderKnobGlow, ColorConvertFloat4ToU32(FromHex(Theme::AccentGlow)));
        dl->AddCircleFilled(ImVec2(knob.x, knob.y + Theme::px(1.0f)), Theme::M::SliderKnob, IM_COL32(0, 0, 0, 55));
        dl->AddCircleFilled(knob, Theme::M::SliderKnob, GetColorU32(ImGuiCol_SliderGrab));

        return changed;
    }

    bool RowInputInt(const char* label, int* v, int v_min, int v_max, int step, int fastStep)
    {
        using namespace ImGui;
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        const float rowH    = Theme::M::InputRowH;
        const float w       = GetContentRegionAvail().x;
        const float inputW  = Theme::M::InputW;
        const ImVec2 origin = window->DC.CursorPos;

        const ImRect rowBB(origin, ImVec2(origin.x + w, origin.y + rowH));
        ItemSize(rowBB);

        const ImVec2 labelSz = CalcTextSize(label);
        RenderText(ImVec2(origin.x, origin.y + (rowH - labelSz.y) * 0.5f), label);

        ImGuiContext& g = *GImGui;
        const float btn = GetFrameHeight();
        const float gap = g.Style.ItemInnerSpacing.x;
        const float fieldW = inputW - 2.0f * (btn + gap);
        SetCursorScreenPos(ImVec2(origin.x + w - inputW, origin.y + (rowH - GetFrameHeight()) * 0.5f));
        SetNextItemWidth(fieldW);

        char hidden[64];
        snprintf(hidden, sizeof(hidden), "##%s", label);
        bool changed = InputInt(hidden, v, step, fastStep, ImGuiInputTextFlags_CharsDecimal);
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

        const float rowH    = Theme::M::InputRowH;
        const float w       = GetContentRegionAvail().x;
        const float gutter  = Theme::M::InputPairGap;
        const float halfW   = (w - gutter) * 0.5f;
        const float inputW  = Theme::M::InputPairW;
        const ImVec2 origin = window->DC.CursorPos;

        const ImRect rowBB(origin, ImVec2(origin.x + w, origin.y + rowH));
        ItemSize(rowBB);

        ImGuiContext& g = *GImGui;
        const float btn = GetFrameHeight();
        const float gap = g.Style.ItemInnerSpacing.x;
        const float fieldW = inputW - 2.0f * (btn + gap);

        auto drawHalf = [&](float originX, const char* label, int* v, int vMin, int vMax, int step, int fast) -> bool {
            const ImVec2 labelSz = CalcTextSize(label);
            RenderText(ImVec2(originX, origin.y + (rowH - labelSz.y) * 0.5f), label);

            SetCursorScreenPos(ImVec2(originX + halfW - inputW, origin.y + (rowH - GetFrameHeight()) * 0.5f));
            SetNextItemWidth(fieldW);

            char hidden[64];
            snprintf(hidden, sizeof(hidden), "##%s", label);
            bool changed = InputInt(hidden, v, step, fast, ImGuiInputTextFlags_CharsDecimal);
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
        ImVec2 size(window->Size.x, Theme::M::TabH);
        ImRect bb(p, ImVec2(p.x + size.x, p.y + size.y));

        ImGuiID id = window->GetID(label);
        ItemSize(bb);
        if (!ItemAdd(bb, id)) return false;

        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held);
        if (hovered) SetMouseCursor(ImGuiMouseCursor_Hand);

        ImDrawList* dl = window->DrawList;
        const float  mx = Theme::M::TabMarginX;
        const float  my = Theme::M::TabMarginY;
        const ImVec2 rMin(bb.Min.x + mx, bb.Min.y + my);
        const ImVec2 rMax(bb.Max.x - mx, bb.Max.y - my);
        const float  rr = Theme::M::TabRound;

        if (selected) {
            dl->AddRectFilled(rMin, rMax, ColorConvertFloat4ToU32(FromHex(Theme::TabSelectedFill)), rr);
        } else if (hovered) {
            dl->AddRectFilled(rMin, rMax, ColorConvertFloat4ToU32(FromHex(Theme::TabHover)), rr);
        }

        const ImU32 textCol = ColorConvertFloat4ToU32(selected ? FromHex(Theme::TabTextActive) : FromHex(Theme::TabTextInactive));

        PushStyleColor(ImGuiCol_Text, textCol);

        ImVec2 labelSz = CalcTextSize(label);
        RenderText(ImVec2(bb.Min.x + Theme::M::TabTextPadX, bb.GetCenter().y - labelSz.y * 0.5f), label);

        PopStyleColor();
        return pressed;
    }
}