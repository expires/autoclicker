#include "Overlay.h"
#include <Windows.h>
#include <gl/GL.h>
#include <climits>
#include <cmath>
#include <mutex>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
#include "../Settings.h"
#include "../modules/esp/EspModule.h"
#include "../SDK/Lunar.h"
#include "../SDK/Minecraft.h"
#include "../SDK/Vec3.h"
#include <MinHook.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Declared manually — intentionally commented out in imgui_impl_win32.h to avoid <windows.h> dependency
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef BOOL(WINAPI* fn_wglSwapBuffers)(HDC);
static fn_wglSwapBuffers o_wglSwapBuffers = nullptr;

// While the overlay is up we no-op all four of these so GLFW's cursor-
// disabled mode can't (a) snap the cursor to center, (b) re-clip it to the
// window, (c) hide it via ShowCursor refcount, or (d) blank the cursor
// image via SetCursor(NULL). Together those four make up the "jitter and
// loss of control" symptom — GLFW fights every frame to reapply its
// disabled-cursor invariant.
typedef BOOL    (WINAPI* fn_SetCursorPos)(int, int);
typedef BOOL    (WINAPI* fn_ClipCursor)(const RECT*);
typedef int     (WINAPI* fn_ShowCursor)(BOOL);
typedef HCURSOR (WINAPI* fn_SetCursor)(HCURSOR);
static fn_SetCursorPos o_SetCursorPos = nullptr;
static fn_ClipCursor   o_ClipCursor   = nullptr;
static fn_ShowCursor   o_ShowCursor   = nullptr;
static fn_SetCursor    o_SetCursor    = nullptr;

static bool    s_initialized        = false;
static bool    s_visible            = false;
static int     s_currentTab         = 0; // 0=Autoclicker, 1=Aim, 2=ESP, 3=Macros, 4=Clans, 5=Settings
static HWND    s_hwnd               = nullptr;
static WNDPROC s_origProc           = nullptr;
// Set when ESC is used to close the menu. Keeps the WndProc swallowing ESC
// messages even after s_visible flips to false, until the key is physically
// released — otherwise the auto-repeat WM_KEYDOWNs that arrive on the same
// frame leak through to MC and open its pause menu.
static bool    s_eatEscUntilRelease = false;

// Set by RowKeybind during render whenever it's in "press a key" mode. The
// toggle handler reads it at the start of the next frame to suppress ESC
// close (so ESC can be used as a cancel inside the keybind picker without
// also slamming the menu shut).
static bool    s_keybindListening   = false;

static ImVec4 FromHex(uint32_t hex, float alpha = 1.0f)
{
    return ImVec4(
        ((hex >> 16) & 0xFF) / 255.0f,
        ((hex >>  8) & 0xFF) / 255.0f,
        ( hex        & 0xFF) / 255.0f,
        alpha);
}

// Byte-wise lerp for ImU32 ABGR colors. Used to animate widget fill colors
// between off / on the same way the reference GUI does via its `Scheme` color
// + FastColorLerp.
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

// Full-row checkbox matching the reference GUI: label on the left, 20-px
// square checkbox on the right, animated fill color + checkmark, bottom
// 1-px border that visually chains adjacent rows into a continuous list.
static bool RowCheckbox(const char* label, bool* v)
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

    // Per-row animation toward 0/1, persisted in the window's storage so it
    // survives across frames without a global map.
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

// Pretty name for a VK_* code. Uses GetKeyNameTextA + a scancode round-trip;
// handles the navigation-cluster keys that need the extended bit set in
// lParam to come back as "Insert" / "Home" / etc. instead of "Num 0".
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

// Full-row key-bind picker. Click the right-hand pill to enter listening
// mode (the next non-mouse key press wins; ESC cancels). Right-click the
// pill to clear back to "none". Returns true on the frame the binding
// changes so the caller can persist immediately.
//
// Active state is a single static — only one picker can listen at a time,
// across all tabs. No per-widget storage means there's no way for a picker
// to be "stuck listening" after the menu closes, switches tabs, etc; ESC
// and a second pill-click both always bail.
//
// s_kbExcluded snapshots which VKs were held at the instant the user
// clicked the pill — those can't bind until the user has physically released
// them. Replaces the prior "arming gate" (which required ALL keys to be
// released globally before any could bind, and could deadlock if a single
// key reported held). Each excluded key clears the moment it's released, so
// a fresh press binds it.
static ImGuiID s_kbActiveId      = 0;
static bool    s_kbExcluded[256] = {};
// allowMouse=true lets the picker bind middle / X1 / X2 mouse buttons.
// LMB and RMB are still filtered unconditionally — clicking the pill itself
// uses LMB, and RMB on the pill is the "clear binding" gesture. The bindings
// that this opens up are useful for in-world toggles (friend-toggle on MMB
// is the asking use-case) where the in-menu keystrokes don't apply because
// the consuming module already gates on Overlay::IsMenuVisible().
static bool RowKeybind(const char* label, int* vk, bool allowMouse = false)
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

    // Right-hand pill — the clickable area for picking the key.
    const float pillW = 120.0f;
    const ImRect pill(ImVec2(bb.Max.x - pillW, bb.Min.y + 4.0f),
                      ImVec2(bb.Max.x,         bb.Max.y - 4.0f));

    bool hovered, held;
    bool pressed = ButtonBehavior(pill, id, &hovered, &held);

    bool changed = false;

    auto isFilteredVk = [allowMouse](int k) {
        // LMB / RMB always filtered: LMB is how the user clicks the pill to
        // activate the picker, RMB on the pill is the unbind gesture. Letting
        // either bind would deadlock the UI.
        if (k == VK_LBUTTON || k == VK_RBUTTON) return true;
        // MMB / XBUTTONs: filtered by default to keep menu input clean, but
        // allowed when the caller opts in (used by the friend-toggle key so
        // the user can bind MMB to "add hovered player as friend").
        if (!allowMouse &&
            (k == VK_MBUTTON || k == VK_XBUTTON1 || k == VK_XBUTTON2))
            return true;
        // Side-agnostic SHIFT/CONTROL/MENU: pressing R-Shift sets BOTH
        // VK_SHIFT (0x10) and VK_RSHIFT (0xA1), so a low-to-high scan
        // would always bind the ambiguous generic VK and the runtime
        // poll would then fire on either side. Filter the generic forms
        // so only the explicit L/R variants can bind — preserves the
        // user's intent to bind specifically RShift vs LShift.
        if (k == VK_SHIFT || k == VK_CONTROL || k == VK_MENU) return true;
        return false;
    };

    // Click handling: clicking the pill activates this picker. Clicking it
    // again while already active cancels — gives the user an escape hatch
    // that doesn't depend on any key being readable.
    if (pressed) {
        if (s_kbActiveId == id) {
            s_kbActiveId = 0;
        } else {
            s_kbActiveId = id;
            // Freeze the current "held" set into the exclusion list. The LMB
            // release that just fired this click, any modifier the user is
            // holding for movement, the menu key still down from opening the
            // overlay — all of these get excluded until physically released.
            // Without this snapshot, the next scan would instantly bind to
            // whichever of them sat lowest in VK order.
            for (int k = 0; k < 256; ++k)
                s_kbExcluded[k] = (GetAsyncKeyState(k) & 0x8000) != 0;
        }
    }

    // Right-click on the pill always clears the binding back to none.
    if (IsMouseHoveringRect(pill.Min, pill.Max) &&
        IsMouseClicked(ImGuiMouseButton_Right)) {
        if (*vk != 0) { *vk = 0; changed = true; }
        if (s_kbActiveId == id) s_kbActiveId = 0;
    }

    const bool listening = (s_kbActiveId == id);

    if (listening) {
        s_keybindListening = true;

        // ESC always bails — works regardless of any key's state. Doesn't
        // clear the existing binding (right-click is the explicit unbind).
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            s_kbActiveId = 0;
        }
        else {
            // Decay the exclusion set: any key that's been released since
            // the click is now a valid fresh-press candidate.
            for (int k = 0; k < 256; ++k) {
                if (s_kbExcluded[k] && !(GetAsyncKeyState(k) & 0x8000))
                    s_kbExcluded[k] = false;
            }

            // First held, non-excluded, bindable key wins. Menu key is
            // explicitly skipped — letting it bind would create a conflict
            // (the same key would open the overlay AND fire the bound action).
            const int menuVk = (g_settings.menuKey > 0 && g_settings.menuKey <= 0xFE)
                ? g_settings.menuKey : VK_RSHIFT;
            // Loop start drops to 0x01 when mouse binds are allowed so
            // VK_MBUTTON (0x04) and the XBUTTONs (0x05/0x06) come into
            // range; isFilteredVk still rejects the always-unbindable
            // LMB/RMB.
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

    // Label on the left.
    ImVec2 labelSz = CalcTextSize(label, nullptr, true);
    PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_Text));
    RenderText(ImVec2(bb.Min.x, bb.GetCenter().y - labelSz.y * 0.5f), label);
    PopStyleColor();

    // Pill background + text.
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

// Full-row int slider matching the reference: label top-left, value top-right,
// thin track + circular grab below, animated fill width, bottom border.
static bool RowSlider(const char* label, int* v, int v_min, int v_max,
                      const char* fmt = "%d")
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

    // Hit area is intentionally larger than the visual track. The 5-px-tall
    // track stays for the look, but landing a click on 5 pixels is fiddly —
    // miss by one above/below and the press hits empty space. Expand the
    // hit rect to cover the whole row below the label so the user can grab
    // the slider anywhere near the track, not just on the dot.
    const ImRect hitFrame(ImVec2(total.Min.x, total.Min.y + labelSz.y + 4.0f),
                          ImVec2(total.Max.x, total.Max.y - 4.0f));

    ImGuiID id = window->GetID(label);
    ItemSize(total, g.Style.FramePadding.y);
    if (!ItemAdd(total, id, &hitFrame)) return false;

    // SliderBehavior only operates while ActiveID == id; without a click
    // hook nothing would ever start dragging. Use ButtonBehavior on the
    // expanded hit rect to set ActiveID on press and hold it while dragging.
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

// InputInt sized to fit on the right of a chained row: label on the left,
// numeric box with +/- step buttons on the right. Step / fast-step are
// configurable so cooldown rows in the millisecond range get useful
// increments (100 / 1000) rather than the +1 default. v_min / v_max clamp
// on commit so a hand-typed value can't escape the slider's old range
// guarantees. No bottom border — the other Row* helpers don't draw one
// either, and the visual mismatch looked off when two InputInt rows sat
// next to a borderless Checkbox above and Keybind below.
static bool RowInputInt(const char* label, int* v,
                        int v_min, int v_max,
                        int step = 1, int fastStep = 10)
{
    using namespace ImGui;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    const float rowH    = 36.0f;
    const float w       = GetContentRegionAvail().x;
    const float inputW  = 160.0f;
    const ImVec2 origin = window->DC.CursorPos;

    // Reserve the full row up front so the bottom-border draw below sits at
    // a known Y. ItemAdd with a no-op ID — the InputInt below carries its
    // own ID derived from the label.
    const ImRect rowBB(origin, ImVec2(origin.x + w, origin.y + rowH));
    ItemSize(rowBB);

    // Label drawn at the row's vertical center, left-aligned. CalcTextSize
    // is height-only here; horizontal sizing is the natural label width.
    const ImVec2 labelSz = CalcTextSize(label);
    RenderText(ImVec2(origin.x, origin.y + (rowH - labelSz.y) * 0.5f), label);

    // Position the InputInt flush to the right. The +/- step buttons are
    // appended after the field by InputInt itself, so we hand it the field
    // width minus the buttons (~2 * (GetFrameHeight() + ItemInnerSpacing.x))
    // to keep total geometry under inputW.
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

// Two labeled InputInts side-by-side in a single row. Each half splits the
// row width evenly (label left, narrower input on the right with +/- step
// buttons). Used by Auto Ability so the Delay and Cooldown rows read as the
// related pair they are rather than two stacked full-width rows. Same
// clamp-on-commit semantics as RowInputInt.
static bool RowInputIntPair(const char* labelA, int* vA, int minA, int maxA, int stepA, int fastA,
                            const char* labelB, int* vB, int minB, int maxB, int stepB, int fastB)
{
    using namespace ImGui;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    const float rowH    = 36.0f;
    const float w       = GetContentRegionAvail().x;
    // 6px gutter between the two halves so the inputs don't visually fuse.
    const float gutter  = 6.0f;
    const float halfW   = (w - gutter) * 0.5f;
    // Input cell is ~110px (narrower than RowInputInt's 160 so the label has
    // room on the left side of the half). +/- buttons still come out of this
    // budget — fieldW below subtracts them.
    const float inputW  = 110.0f;
    const ImVec2 origin = window->DC.CursorPos;

    const ImRect rowBB(origin, ImVec2(origin.x + w, origin.y + rowH));
    ItemSize(rowBB);

    ImGuiContext& g = *GImGui;
    const float btn = GetFrameHeight();
    const float gap = g.Style.ItemInnerSpacing.x;
    const float fieldW = inputW - 2.0f * (btn + gap);

    // Shared draw routine for one half. originX is the left edge of the half;
    // label sits at originX vertically centered, input flushes to originX+halfW.
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

    bool changedA = drawHalf(origin.x,                       labelA, vA, minA, maxA, stepA, fastA);
    bool changedB = drawHalf(origin.x + halfW + gutter,      labelB, vB, minB, maxB, stepB, fastB);

    return changedA || changedB;
}

// Full-width clickable row used as a sidebar tab item: a label and a 2-px
// full-height accent stripe along the right edge that slides between tabs
// when the selection changes. Mirrors the `custom::tab` style from the
// reference GUI in .temp/.../custom.cpp.
//
// The accent stripe uses a static `line_pos` shared across every tab call —
// only the selected tab updates the target, all tabs paint at the same Y so
// the stripe visually "moves" between them as line_pos lerps.
static bool SidebarTab(const char* label, bool selected)
{
    using namespace ImGui;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImVec2 p = window->DC.CursorPos;
    // 32px tall — trimmed from 38 so all seven tabs (Autoclicker, Aim,
    // ESP, Friends, Macros, Clans, Settings) fit inside the 380px window
    // without the last one clipping off the bottom edge.
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

    // Animated accent stripe — full-height, 2-px wide, rounded-left.
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

static void ApplyStyle()
{
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowPadding      = {12.0f, 12.0f};
    s.FramePadding       = { 6.0f,  3.0f};
    s.ItemSpacing        = { 8.0f,  7.0f};
    s.ItemInnerSpacing   = { 6.0f,  4.0f};
    s.IndentSpacing      = 14.0f;
    s.WindowRounding     = 8.0f;
    s.FrameRounding      = 4.0f;
    s.GrabRounding       = 6.0f;
    // Chrome-style tabs: rounded top, the active tab blends into the panel
    // below, inactive tabs sit on a darker bar.
    s.TabRounding        = 8.0f;
    s.TabBarBorderSize   = 0.0f;
    s.TabBorderSize      = 0.0f;
    s.ScrollbarRounding  = 4.0f;
    // Slimmer than the ImGui default (~14). Keeps the visual weight of the
    // scrollbar low so it can sit close to the panel edge without dominating
    // the rounded corner.
    s.ScrollbarSize      = 10.0f;
    s.ChildRounding      = 6.0f;
    s.PopupRounding      = 4.0f;
    s.WindowBorderSize   = 1.0f;
    s.ChildBorderSize    = 1.0f;
    s.FrameBorderSize    = 0.0f;
    s.WindowMinSize      = {220.0f, 80.0f};

    ImVec4* c = s.Colors;

    c[ImGuiCol_WindowBg]              = FromHex(0x0b0f17);
    c[ImGuiCol_ChildBg]               = FromHex(0x10151f);
    c[ImGuiCol_PopupBg]               = FromHex(0x10151f);

    c[ImGuiCol_Border]                = FromHex(0x1e2535);
    c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);

    c[ImGuiCol_FrameBg]               = FromHex(0x161d2e);
    c[ImGuiCol_FrameBgHovered]        = FromHex(0x1d2438);
    c[ImGuiCol_FrameBgActive]         = FromHex(0x232b42);

    c[ImGuiCol_TitleBg]               = FromHex(0x0b0f17);
    c[ImGuiCol_TitleBgActive]         = FromHex(0x10151f);
    c[ImGuiCol_TitleBgCollapsed]      = FromHex(0x0b0f17);

    c[ImGuiCol_CheckMark]             = FromHex(0xffffff);
    c[ImGuiCol_SliderGrab]            = FromHex(0x5865f2);
    c[ImGuiCol_SliderGrabActive]      = FromHex(0x6b76f3);

    c[ImGuiCol_Button]                = FromHex(0x1a2138);
    c[ImGuiCol_ButtonHovered]         = FromHex(0x202845);
    c[ImGuiCol_ButtonActive]          = FromHex(0x2a3358);

    c[ImGuiCol_Header]                = FromHex(0x5865f2, 0.30f);
    c[ImGuiCol_HeaderHovered]         = FromHex(0x5865f2, 0.50f);
    c[ImGuiCol_HeaderActive]          = FromHex(0x5865f2, 0.70f);

    c[ImGuiCol_Separator]             = FromHex(0x1e2535);
    c[ImGuiCol_SeparatorHovered]      = FromHex(0x5865f2, 0.40f);
    c[ImGuiCol_SeparatorActive]       = FromHex(0x5865f2, 0.80f);

    // Inactive tabs dark (matching WindowBg), active tab clearly lit — the
    // selected one is the one that stands out as the brighter "card".
    c[ImGuiCol_Tab]                   = FromHex(0x0b0f17); // inactive: blends with bg
    c[ImGuiCol_TabHovered]            = FromHex(0x1d2438);
    c[ImGuiCol_TabActive]             = FromHex(0x161d2e); // selected: brighter
    c[ImGuiCol_TabUnfocused]          = FromHex(0x0b0f17);
    c[ImGuiCol_TabUnfocusedActive]    = FromHex(0x161d2e);
    c[ImGuiCol_TabSelectedOverline]   = ImVec4(0, 0, 0, 0); // no overline

    c[ImGuiCol_ScrollbarBg]           = FromHex(0x0b0f17);
    c[ImGuiCol_ScrollbarGrab]         = FromHex(0x1e2535);
    c[ImGuiCol_ScrollbarGrabHovered]  = FromHex(0x5865f2, 0.40f);
    c[ImGuiCol_ScrollbarGrabActive]   = FromHex(0x5865f2, 0.60f);

    c[ImGuiCol_ResizeGrip]            = FromHex(0x5865f2, 0.15f);
    c[ImGuiCol_ResizeGripHovered]     = FromHex(0x5865f2, 0.50f);
    c[ImGuiCol_ResizeGripActive]      = FromHex(0x5865f2, 0.80f);

    c[ImGuiCol_Text]                  = FromHex(0xc5cdd9);
    c[ImGuiCol_TextDisabled]          = FromHex(0x606878);
}

// Project a world point into screen space using MC's yaw/pitch convention.
// Returns false if the point is behind the camera.
static bool ProjectWorld(double wx, double wy, double wz,
                         const EspModule::CameraState& cam,
                         float dispW, float dispH, ImVec2& out)
{
    double dx = wx - cam.x;
    double dy = wy - cam.y;
    double dz = wz - cam.z;

    double yawRad   = cam.yRot * M_PI / 180.0;
    double pitchRad = cam.xRot * M_PI / 180.0;

    // Undo yaw rotation around Y
    double cy = std::cos(yawRad);
    double sy = std::sin(yawRad);
    double x1 =  dx * cy + dz * sy;
    double z1 = -dx * sy + dz * cy;
    double y1 =  dy;

    // Undo pitch rotation around X
    double cp = std::cos(pitchRad);
    double sp = std::sin(pitchRad);
    double y2 =  y1 * cp + z1 * sp;
    double z2 = -y1 * sp + z1 * cp;
    double x2 =  x1;

    if (z2 <= 0.1) return false;

    double fovRad = cam.fov * M_PI / 180.0;
    if (fovRad <= 0.01) fovRad = 70.0 * M_PI / 180.0;
    double f = 1.0 / std::tan(fovRad / 2.0);
    double aspect = (double)dispW / (double)dispH;

    // Sign flip on X because MC's +X at yaw=0 sits to the player's left.
    out.x = (float)(dispW * 0.5 - (x2 / z2) * f / aspect * dispW * 0.5);
    out.y = (float)(dispH * 0.5 - (y2 / z2) * f                * dispH * 0.5);
    return true;
}

// MC's main render thread is already attached to the JVM (MC put it there).
// We just need our thread_local lc->env populated so SDK calls work — grab
// the existing env via GetEnv (which does NOT attach, so we can't accidentally
// own / detach MC's thread). One-shot per thread; subsequent calls early-out
// on the lc->env != nullptr check.
static void EnsureRenderThreadEnv()
{
    if (lc->env != nullptr) return;
    if (lc->vm  == nullptr) return;
    if (!lc->classesLoaded.load(std::memory_order_acquire)) return;
    JNIEnv* env = nullptr;
    if (lc->vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK && env)
        lc->env = env;
}

// Read camera position / rotation / FOV + partial tick live from MC's render
// thread, overwriting the snapshot's cached values. The worker-thread snapshot
// reads these at random times relative to MC's render frame, so during fast
// camera motion (jumping, sprinting) the cached camera lags reality by a few
// ms — the ESP boxes then appear to bob up/down because we're projecting
// entities against a stale camera while MC draws the world against the fresh
// one. Re-reading on the render thread closes that gap: snap.cam now matches
// the exact camera state MC just used to render this frame.
//
// Entity positions stay snapshot-sourced (tick-domain, cheap to read async),
// but we DO refresh partialTick — using fresh PT with stale xo/x lerps the
// entity to where MC just rendered it, eliminating "jelly target" artifacts
// where the box trails the actual player by a few frames.
static void RefreshCameraFromRenderThread(EspModule::Snapshot& snap)
{
    EnsureRenderThreadEnv();
    if (lc->env == nullptr) return;
    if (lc->env->PushLocalFrame(32) != 0) { lc->env->ExceptionClear(); return; }

    Minecraft mc;
    if (jobject mcInst = mc.GetInstance()) {
        // Read partial tick FIRST — getFov() interpolates by partialTick for
        // FOV transitions (sprint start/stop, eating, hurt anim, bow draw),
        // so passing the same pt MC just used for its world render is what
        // syncs our projection to the on-screen world. The old code passed
        // a hardcoded 1.0 here, which meant during a sprint→walk FOV
        // transition (triggered by colliding with a wall) MC's world drew
        // at the in-transition FOV while our boxes projected at the fully-
        // transitioned target FOV — boxes drifted relative to entities for
        // a few frames each collision, reading as jitter.
        float pt = 1.0f;
        DeltaTracker dt = mc.GetDeltaTracker();
        if (dt.GetInstance() != nullptr) {
            float ptRead = dt.getPartialTick(true);
            if (!lc->env->ExceptionCheck()) {
                pt = ptRead;
                snap.partialTick = ptRead;
            }
        }

        GameRenderer gr = mc.GetGameRenderer();
        if (gr.GetInstance() != nullptr) {
            Camera cam = gr.getMainCamera();
            if (cam.GetInstance() != nullptr) {
                Vec3 cp = cam.getPosition();
                if (cp.GetInstance() != nullptr) {
                    snap.cam.x = cp.getX();
                    snap.cam.y = cp.getY();
                    snap.cam.z = cp.getZ();
                }
                snap.cam.yRot = cam.getYRot();
                snap.cam.xRot = cam.getXRot();
                float fov = gr.getFov(cam, pt, true);
                if (!lc->env->ExceptionCheck() && fov > 0.0f) snap.cam.fov = fov;
            }
        }
    }

    if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
    lc->env->PopLocalFrame(nullptr);
}

static void DrawEsp(float dispW, float dispH)
{
    EspModule::Snapshot snap;
    {
        std::lock_guard<std::mutex> lk(EspModule::snapMutex);
        if (!EspModule::snapshot.valid) return;
        snap = EspModule::snapshot;
    }

    // Live camera read on the render thread — kills the jump-induced
    // box-bobbing caused by snap.cam drift vs. MC's actual frame camera.
    RefreshCameraFromRenderThread(snap);

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float maxDistSq = (float)g_settings.maxDistance * (float)g_settings.maxDistance;

    const double pt = (double)snap.partialTick;

    for (const auto& t : snap.targets)
    {
        // Interpolate entity position the same way MC does at render time.
        // Camera.position was already interpolated by Camera.setup(), so the
        // two now share the same frame's partial tick.
        const double ix = t.prevX + (t.x - t.prevX) * pt;
        const double iy = t.prevY + (t.y - t.prevY) * pt;
        const double iz = t.prevZ + (t.z - t.prevZ) * pt;

        double ddx = ix - snap.cam.x;
        double ddy = iy - snap.cam.y;
        double ddz = iz - snap.cam.z;
        double distSq = ddx*ddx + ddy*ddy + ddz*ddz;
        if (distSq > maxDistSq) continue;

        // Rebuild AABB around the interpolated feet position.
        const double minX = ix - t.halfWidth, maxX = ix + t.halfWidth;
        const double minY = iy,               maxY = iy + t.height;
        const double minZ = iz - t.halfDepth, maxZ = iz + t.halfDepth;

        const double cx[2] = {minX, maxX};
        const double cy[2] = {minY, maxY};
        const double cz[2] = {minZ, maxZ};

        // Project all 8 corners. corner[i] = (x_bit | y_bit<<1 | z_bit<<2):
        //   0 = (-x, -y, -z)   4 = (-x, -y, +z)
        //   1 = (+x, -y, -z)   5 = (+x, -y, +z)
        //   2 = (-x, +y, -z)   6 = (-x, +y, +z)
        //   3 = (+x, +y, -z)   7 = (+x, +y, +z)
        ImVec2 sp[8];
        bool   ok[8];
        float minSX =  1e9f, minSY =  1e9f;
        float maxSX = -1e9f, maxSY = -1e9f;
        bool any = false;

        for (int i = 0; i < 8; ++i)
        {
            double wx = cx[(i >> 0) & 1];
            double wy = cy[(i >> 1) & 1];
            double wz = cz[(i >> 2) & 1];
            ImVec2 p;
            ok[i] = ProjectWorld(wx, wy, wz, snap.cam, dispW, dispH, p);
            if (!ok[i]) continue;
            // Snap to integer pixels. Without this, the 1.8px anti-aliased
            // lines pick up float-precision drift in the projection math —
            // every frame the line endpoints round differently into the
            // rasterizer and the cuboid visibly shimmers, especially during
            // wall-press where the camera is held against a collision and
            // the only motion is sub-pixel projection noise. Pixel snapping
            // costs visible-smooth-motion at the corner of a moving target
            // (it now steps frame-by-frame instead of sliding), but stops
            // the shake completely.
            sp[i].x = std::floor(p.x + 0.5f);
            sp[i].y = std::floor(p.y + 0.5f);
            any = true;
            if (sp[i].x < minSX) minSX = sp[i].x;
            if (sp[i].y < minSY) minSY = sp[i].y;
            if (sp[i].x > maxSX) maxSX = sp[i].x;
            if (sp[i].y > maxSY) maxSY = sp[i].y;
        }
        if (!any) continue;

        // Clip insane offscreen values
        if (maxSX < 0 || minSX > dispW || maxSY < 0 || minSY > dispH) continue;

        // Friend tint — bright lime green. Override the team-derived box
        // and nametag colors so a friend pops visually no matter what team
        // color the server assigned. Gated by highlightFriends so the user
        // can opt out (e.g. for a screenshot where they want pure team
        // colors but still want the friends list functional elsewhere).
        const bool tintFriend = t.isFriend && g_settings.highlightFriends;
        const ImU32 friendCol = IM_COL32(85, 255, 85, 255);

        if (g_settings.drawBox)
        {
            // 3D wireframe — 12 edges of the AABB cuboid, like MC's F3+B
            // hitbox overlay. Each edge connects two of the 8 corners; we
            // skip an edge if either endpoint failed to project (behind near
            // plane). Listed in (corner_a, corner_b) pairs:
            //   bottom face (y=min):     0-1, 1-3, 3-2, 2-0
            //   top face    (y=max):     4-5, 5-7, 7-6, 6-4
            //   vertical pillars:        0-4, 1-5, 2-6, 3-7
            static const int edges[12][2] = {
                {0,1},{1,3},{3,2},{2,0},
                {4,5},{5,7},{7,6},{6,4},
                {0,4},{1,5},{2,6},{3,7},
            };
            const uint32_t srcCol = tintFriend ? friendCol : t.boxColor;
            // Alpha ~220 to match the prior outline look. 1.8px line weight
            // keeps the 12-edge cuboid readable at distance without going
            // full crayon — 1.0 read too thin per user, 2.5 too cartoonish.
            const ImU32 colBox = (srcCol & 0x00FFFFFFu) | (220u << 24);
            for (int e = 0; e < 12; ++e) {
                const int a = edges[e][0], b = edges[e][1];
                if (!ok[a] || !ok[b]) continue;
                dl->AddLine(sp[a], sp[b], colBox, 1.8f);
            }
        }

        if (g_settings.drawName || g_settings.drawDistance || g_settings.drawHealth)
        {
            // Build the list of colored chunks we'll render. drawName toggles
            // include the team-formatted player chunks; drawDistance appends
            // a final dimmed chunk with the distance; drawHealth appends
            // an HP chunk with a red→yellow→green ramp.
            struct Chunk { std::string text; ImU32 col; };
            std::vector<Chunk> chunks;
            if (g_settings.drawName) {
                for (const auto& nc : t.nameChunks)
                    if (!nc.first.empty()) {
                        // Friend tint replaces every name chunk's color so
                        // multi-color guild prefixes don't drown the signal.
                        const ImU32 col = tintFriend ? friendCol : (ImU32)nc.second;
                        chunks.push_back({nc.first, col});
                    }
            }
            if (g_settings.drawHealth && t.health >= 0.0f && t.maxHealth > 0.0f) {
                // Display in HP units (not hearts) so the value matches what
                // sandbox/debug tools / commands report. Ramp the color by
                // health fraction: <30% red, <70% yellow, else green — quick
                // visual read of whether a player is finishable.
                char hpBuf[24];
                snprintf(hpBuf, sizeof(hpBuf), " %.0f/%.0f",
                         t.health, t.maxHealth);
                const float frac = t.health / t.maxHealth;
                ImU32 hpCol;
                if (frac < 0.30f)      hpCol = IM_COL32(255,  80,  80, 230);
                else if (frac < 0.70f) hpCol = IM_COL32(255, 210,  90, 230);
                else                   hpCol = IM_COL32(120, 230, 120, 230);
                chunks.push_back({hpBuf, hpCol});
            }
            if (g_settings.drawDistance) {
                char distBuf[16];
                snprintf(distBuf, sizeof(distBuf), " %.1fm", (float)std::sqrt(distSq));
                chunks.push_back({distBuf, IM_COL32(170, 200, 255, 220)});
            }
            if (chunks.empty()) continue;

            // World-locked anchor: MC renders its nametag at
            //   entity.y + entity.height + 0.5
            // Project that point straight to screen.
            ImVec2 tagPos;
            if (!ProjectWorld(ix, iy + t.height + 0.5, iz, snap.cam, dispW, dispH, tagPos))
                continue;

            // Scale with distance the way MC's nametag does so the pill stays
            // visually proportional to the player model. World-space text
            // height ~0.225 blocks; projected to screen pixels with the same
            // focal length as our box. 1.2× on top makes it slightly larger
            // than vanilla. Floor at 14px keeps it readable far away; no
            // upper cap so it grows naturally as you approach.
            constexpr float NAMETAG_Y_SHIFT_PX = 4.0f;
            constexpr float WORLD_TEXT_HEIGHT  = 0.225f;
            constexpr float FONT_SCALE         = 1.2f;
            double fovRad = snap.cam.fov * M_PI / 180.0;
            if (fovRad <= 0.01) fovRad = 70.0 * M_PI / 180.0;
            double f = 1.0 / std::tan(fovRad / 2.0);
            const double dist3D = std::sqrt(distSq);
            float fontSize = (float)(WORLD_TEXT_HEIGHT * f * dispH * 0.5 / dist3D) * FONT_SCALE;
            if (fontSize < 14.0f) fontSize = 14.0f;

            ImFont* font = ImGui::GetFont();
            float contentW = 0.0f;
            float contentH = 0.0f;
            for (auto& c : chunks) {
                ImVec2 sz = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, c.text.c_str());
                contentW += sz.x;
                if (sz.y > contentH) contentH = sz.y;
            }

            const float padX     = fontSize * 0.35f;
            const float padY     = fontSize * 0.12f;
            const float anchorY  = tagPos.y + NAMETAG_Y_SHIFT_PX;
            const float bgLeft   = tagPos.x - contentW * 0.5f - padX;
            const float bgRight  = tagPos.x + contentW * 0.5f + padX;
            const float bgTop    = anchorY - contentH * 0.5f - padY;
            const float bgBottom = anchorY + contentH * 0.5f + padY;
            const float rounding = 4.0f;

            const ImU32 bgCol     = IM_COL32(12,  14, 20, 200);
            const ImU32 borderCol = IM_COL32(80,  90, 110, 180);

            dl->AddRectFilled(ImVec2(bgLeft, bgTop), ImVec2(bgRight, bgBottom), bgCol, rounding);
            dl->AddRect      (ImVec2(bgLeft, bgTop), ImVec2(bgRight, bgBottom), borderCol, rounding, 0, 1.0f);

            float cursorX = bgLeft + padX;
            const float textY = bgTop + padY + (contentH - fontSize) * 0.5f;
            for (auto& c : chunks) {
                dl->AddText(font, fontSize, ImVec2(cursorX, textY), c.col, c.text.c_str());
                cursorX += font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, c.text.c_str()).x;
            }
        }
    }
}

static BOOL WINAPI hk_SetCursorPos(int x, int y)
{
    // Lie to GLFW: it thinks the recenter succeeded, but the cursor stays
    // where the user moved it. No jitter, no fight.
    if (s_visible) return TRUE;
    return o_SetCursorPos(x, y);
}

static BOOL WINAPI hk_ClipCursor(const RECT* r)
{
    // Same idea — GLFW re-clips every frame in cursor-disabled mode.
    if (s_visible) return TRUE;
    return o_ClipCursor(r);
}

static int WINAPI hk_ShowCursor(BOOL show)
{
    // Refuse to hide. GLFW calls ShowCursor(FALSE) once per frame in
    // disabled-cursor mode; without this, the global show-counter oscillates
    // around -1 every frame and the cursor flickers in and out of visibility,
    // which reads as "jitter".
    if (s_visible && !show) return 0;
    return o_ShowCursor(show);
}

static HCURSOR WINAPI hk_SetCursor(HCURSOR cursor)
{
    // GLFW calls SetCursor(NULL) every frame in disabled-cursor mode to
    // blank the cursor image. While our menu is up we substitute IDC_ARROW
    // so the cursor stays visibly drawn. Earlier version returned
    // ::GetCursor() (the previous cursor), but if GLFW had already blanked
    // the cursor before our DLL loaded, "previous" was also NULL — cursor
    // moved invisibly. Calling o_SetCursor explicitly with the arrow
    // resource forces a real cursor image regardless of prior state.
    if (s_visible && cursor == nullptr) {
        // IDC_ARROW = 32512. Use MAKEINTRESOURCEW because the LoadCursorW
        // signature wants LPCWSTR, not the LPSTR that the IDC_ARROW macro
        // expands to in non-UNICODE builds.
        static HCURSOR s_arrow = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        return o_SetCursor(s_arrow);
    }
    return o_SetCursor(cursor);
}

// Raw-input read-time filter. GLFW3 in disabled-cursor mode delivers mouse
// deltas via raw input — either as WM_INPUT messages (our WndProc swallows
// those when s_visible) or via GetRawInputBuffer polling, which entirely
// bypasses WndProc.
//
// First attempt was unregistering the mouse raw-input device on menu open
// (RegisterRawInputDevices + RIDEV_REMOVE). That successfully blocked input,
// but on close GLFW's internal state machine didn't notice that the device
// had been yanked + restored — screen glitched and input was dead until an
// alt-tab forced WM_ACTIVATE to re-init everything. Registration tampering
// is the wrong layer.
//
// Better: leave registrations alone, hook the read functions themselves. When
// s_visible, GetRawInputBuffer reports zero pending events; GetRawInputData
// returns 0 bytes. MC's GLFW still thinks it's registered (no state churn),
// it just gets no input to act on. When the menu closes the hooks pass
// through and the next raw event flows normally — no nudge required.
typedef UINT(WINAPI* fn_GetRawInputBuffer)(PRAWINPUT, PUINT, UINT);
typedef UINT(WINAPI* fn_GetRawInputData)(HRAWINPUT, UINT, LPVOID, PUINT, UINT);
static fn_GetRawInputBuffer o_GetRawInputBuffer = nullptr;
static fn_GetRawInputData   o_GetRawInputData   = nullptr;

// Our own RAWINPUT advance — system NEXTRAWINPUTBLOCK expands to a body
// that uses the QWORD typedef, which isn't defined in all MSVC SDK
// configurations (CI runner trips here). 8 is the correct literal for
// QWORD alignment regardless.
#define ESP_NEXT_RAWINPUT_BLOCK(ptr) \
    ((PRAWINPUT)(((ULONG_PTR)((PBYTE)(ptr) + (ptr)->header.dwSize) + 7) & ~(ULONG_PTR)7))

// Filter-not-block strategy: let raw-input reads complete normally, but
// zero out the mouse delta + button bits in any RIM_TYPEMOUSE records
// while the menu is open. Earlier version returned 0 events for the
// entire buffer — that backed up the kernel's input queue, caused MC's
// input loop to busy-spin (no events but still polling), and stalled
// OS-level cursor tracking → cursor freezes every few seconds. Letting
// events through with zeroed motion drains the pump cleanly and the
// OS keeps updating cursor position normally.

static UINT WINAPI hk_GetRawInputBuffer(PRAWINPUT pData, PUINT pcbSize, UINT cbSizeHeader)
{
    UINT count = o_GetRawInputBuffer(pData, pcbSize, cbSizeHeader);
    if (s_visible && pData && count > 0 && count != (UINT)-1) {
        PRAWINPUT cur = pData;
        for (UINT i = 0; i < count; ++i) {
            if (cur->header.dwType == RIM_TYPEMOUSE) {
                cur->data.mouse.lLastX        = 0;
                cur->data.mouse.lLastY        = 0;
                cur->data.mouse.usButtonFlags = 0;
                cur->data.mouse.usButtonData  = 0;
                cur->data.mouse.ulRawButtons  = 0;
            }
            cur = ESP_NEXT_RAWINPUT_BLOCK(cur);
        }
    }
    return count;
}

static UINT WINAPI hk_GetRawInputData(HRAWINPUT hRawInput, UINT uiCommand,
                                       LPVOID pData, PUINT pcbSize, UINT cbSizeHeader)
{
    UINT result = o_GetRawInputData(hRawInput, uiCommand, pData, pcbSize, cbSizeHeader);
    // Only mutate the actual data read (RID_INPUT). RID_HEADER size queries
    // need to return the real size or callers fail to allocate the buffer.
    if (s_visible && pData && uiCommand == RID_INPUT && result != (UINT)-1) {
        PRAWINPUT ri = (PRAWINPUT)pData;
        if (ri->header.dwType == RIM_TYPEMOUSE) {
            ri->data.mouse.lLastX        = 0;
            ri->data.mouse.lLastY        = 0;
            ri->data.mouse.usButtonFlags = 0;
            ri->data.mouse.usButtonData  = 0;
            ri->data.mouse.ulRawButtons  = 0;
        }
    }
    return result;
}

// Synthesize key-up / button-up messages for everything currently pressed so
// MC's input pump sees them as released. Without this, holding W when you
// press INSERT keeps the player walking forward forever — we swallow the
// real WM_KEYUP later, so MC never sees a release otherwise. We bypass our
// own WndProc hook by calling s_origProc directly.
static void ReleaseAllHeldInputs()
{
    if (!s_hwnd || !s_origProc) return;

    for (int vk = 0x01; vk <= 0xFE; ++vk) {
        // Skip our toggle keys — they're transient and the swallow logic
        // doesn't care about their up edge.
        if (vk == VK_INSERT) continue;
        // Skip mouse buttons; handled with their own messages below.
        if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON ||
            vk == VK_XBUTTON1 || vk == VK_XBUTTON2) continue;

        if (GetAsyncKeyState(vk) & 0x8000) {
            const UINT scan = MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_VSC);
            // lParam bits: repeat=1, scan, prev-state=1, transition=1 (= up)
            const LPARAM lParam =
                1 | (LPARAM(scan) << 16) | (LPARAM(1) << 30) | (LPARAM(1) << 31);
            CallWindowProcW(s_origProc, s_hwnd, WM_KEYUP, (WPARAM)vk, lParam);
        }
    }

    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
        CallWindowProcW(s_origProc, s_hwnd, WM_LBUTTONUP, 0, 0);
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000)
        CallWindowProcW(s_origProc, s_hwnd, WM_RBUTTONUP, 0, 0);
    if (GetAsyncKeyState(VK_MBUTTON) & 0x8000)
        CallWindowProcW(s_origProc, s_hwnd, WM_MBUTTONUP, 0, 0);
}

static LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Eat ESC events even after s_visible flips off, until the user lets go
    // of the key. Without this, the auto-repeat WM_KEYDOWN that arrives in
    // the same frame as our close-via-ESC reaches MC and opens its pause
    // menu over our (now-hidden) overlay.
    if (s_eatEscUntilRelease && wParam == VK_ESCAPE)
    {
        switch (msg)
        {
        case WM_KEYDOWN: case WM_KEYUP:
        case WM_SYSKEYDOWN: case WM_SYSKEYUP:
        case WM_CHAR:    case WM_SYSCHAR: case WM_UNICHAR:
            return 0;
        }
    }

    if (s_visible)
    {
        // Feed every input event to ImGui first so the menu remains usable.
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);

        // Swallow input-bearing messages so MC's GLFW pump never sees them.
        // Tried routing through MC's own pauseGame()/setScreen() to engage a
        // real pause — that crashed the JVM because the call chain
        // (setScreen → Screen.init → KeyMapping.releaseAll → MouseHandler
        // .releaseMouse) hits state that's only safe at specific points in
        // MC's tick, not from inside wglSwapBuffers. WndProc swallow is what
        // every external overlay does (Discord, Steam, RTSS) — it's the
        // standard pattern, not a workaround.
        switch (msg)
        {
        case WM_KEYDOWN:    case WM_KEYUP:
        case WM_SYSKEYDOWN: case WM_SYSKEYUP:
        case WM_CHAR:       case WM_SYSCHAR:        case WM_UNICHAR:
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
        case WM_MOUSEWHEEL:  case WM_MOUSEHWHEEL:
        case WM_INPUT:
            return 0;
        }
    }
    return CallWindowProcW(s_origProc, hwnd, msg, wParam, lParam);
}

static BOOL WINAPI hk_wglSwapBuffers(HDC hdc)
{
    if (!s_initialized)
    {
        s_hwnd = WindowFromDC(hdc);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;

        char winDir[MAX_PATH] = {};
        GetWindowsDirectoryA(winDir, MAX_PATH);

        // Properly null-init the path buffers — strcat_s on uninitialized
        // memory in release builds silently corrupts the path and we end up
        // failing to load any font, leaving the slot missing and crashing
        // the first PushFont call.
        char fontBold[MAX_PATH]      = {};
        char fontSemibold[MAX_PATH]  = {};
        char fontRegular[MAX_PATH]   = {};
        char fontIcons[MAX_PATH]     = {};
        char fontIconsMDL2[MAX_PATH] = {};
        strcpy_s(fontBold,      sizeof(fontBold),      winDir); strcat_s(fontBold,      sizeof(fontBold),      "\\Fonts\\segoeuib.ttf");
        strcpy_s(fontSemibold,  sizeof(fontSemibold),  winDir); strcat_s(fontSemibold,  sizeof(fontSemibold),  "\\Fonts\\seguisb.ttf");
        strcpy_s(fontRegular,   sizeof(fontRegular),   winDir); strcat_s(fontRegular,   sizeof(fontRegular),   "\\Fonts\\segoeui.ttf");
        strcpy_s(fontIcons,     sizeof(fontIcons),     winDir); strcat_s(fontIcons,     sizeof(fontIcons),     "\\Fonts\\SegoeIcons.ttf");  // Win11 Fluent Icons
        strcpy_s(fontIconsMDL2, sizeof(fontIconsMDL2), winDir); strcat_s(fontIconsMDL2, sizeof(fontIconsMDL2), "\\Fonts\\segmdl2.ttf");      // Win10 MDL2 Assets

        ImFontConfig cfg;
        cfg.OversampleH = 3;
        cfg.OversampleV = 3;
        cfg.PixelSnapH  = false;

        // Three slots, always populated (fallback to default font so
        // PushFont(Fonts[N]) is always safe):
        //   0 — body regular
        //   1 — icons (Private Use Area glyphs from Segoe Fluent / MDL2)
        //   2 — bold (sidebar brand + content title)

        // Slot 0 — body. Prefer Segoe UI Semibold (seguisb.ttf): cleaner
        // mid-weight than full Bold, ships with Windows 7+. Falls back to
        // regular if missing.
        const char* bodyPath =
            (GetFileAttributesA(fontSemibold) != INVALID_FILE_ATTRIBUTES) ? fontSemibold :
            (GetFileAttributesA(fontRegular)  != INVALID_FILE_ATTRIBUTES) ? fontRegular  : nullptr;
        ImFont* fReg = bodyPath
            ? io.Fonts->AddFontFromFileTTF(bodyPath, 16.0f, &cfg) : nullptr;
        if (!fReg) io.Fonts->AddFontDefault();

        // Slot 1 — icons. The glyphs we use (target, people, gear) live in
        // the Private Use Area; ImGui's default range is Basic Latin only,
        // so we explicitly request E000-F8FF or the glyphs render as boxes.
        static const ImWchar iconRanges[] = { 0xE000, 0xF8FF, 0 };
        ImFontConfig iconCfg;
        iconCfg.OversampleH = 2;
        iconCfg.OversampleV = 2;
        iconCfg.PixelSnapH  = true;
        const char* iconPath =
            (GetFileAttributesA(fontIcons)     != INVALID_FILE_ATTRIBUTES) ? fontIcons     :
            (GetFileAttributesA(fontIconsMDL2) != INVALID_FILE_ATTRIBUTES) ? fontIconsMDL2 : nullptr;
        ImFont* fIcon = iconPath
            ? io.Fonts->AddFontFromFileTTF(iconPath, 15.0f, &iconCfg, iconRanges) : nullptr;
        if (!fIcon) io.Fonts->AddFontDefault();

        // Slot 2 — bold
        ImFont* fBold = (GetFileAttributesA(fontBold) != INVALID_FILE_ATTRIBUTES)
            ? io.Fonts->AddFontFromFileTTF(fontBold, 19.0f, &cfg) : nullptr;
        if (!fBold) io.Fonts->AddFontDefault();

        ApplyStyle();

        ImGui_ImplWin32_Init(s_hwnd);
        ImGui_ImplOpenGL3_Init(nullptr);

        s_origProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(s_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc)));

        s_initialized = true;
    }

    // Re-hook on HWND change. Lunar / MC sometimes recreate the window
    // (fullscreen toggle, some mod-driven swap-chain rebuilds, resolution
    // change) — when that happens we keep drawing to the new window because
    // o_wglSwapBuffers uses the live hdc, but our WndProc hook is stranded
    // on the OLD window's GWLP_WNDPROC slot. Result: menu draws fine on
    // the new window, but mouse / kbd events on the visible window bypass
    // our swallow path and go straight to MC's GLFW, so the user's screen
    // pans around while the menu sits there uninteractable.
    {
        HWND liveHwnd = WindowFromDC(hdc);
        if (liveHwnd && liveHwnd != s_hwnd) {
            // Best-effort restore on the dead window so we don't leave a
            // function pointer into our DLL behind if the window happens
            // to still be alive somewhere (we'd UAF on DLL unload).
            if (s_hwnd && s_origProc)
                SetWindowLongPtrW(s_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(s_origProc));
            s_hwnd     = liveHwnd;
            s_origProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(s_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc)));
            ImGui_ImplWin32_Shutdown();
            ImGui_ImplWin32_Init(s_hwnd);
        }

        // Same-HWND eviction guard. A Lunar mod (or any other code on the
        // process) can SetWindowLongPtrW(GWLP_WNDPROC, ...) on our window
        // and replace our HookedWndProc without the HWND itself changing —
        // the HWND-change branch above wouldn't catch that. Symptoms are
        // identical (menu draws, input bypasses our swallow). Cheap to
        // verify each frame and re-install if needed.
        if (s_hwnd) {
            WNDPROC current = reinterpret_cast<WNDPROC>(
                GetWindowLongPtrW(s_hwnd, GWLP_WNDPROC));
            if (current && current != HookedWndProc) {
                // Treat the now-active proc as our new origProc (it's the
                // thing we need to call through to — whichever mod just
                // installed itself) and re-chain on top.
                s_origProc = current;
                SetWindowLongPtrW(s_hwnd, GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(HookedWndProc));
            }
        }
    }

    {
        // Snapshot the keybind-listening flag from the previous frame and
        // reset it; this frame's RowKeybind will set it again if any
        // picker is still listening.
        const bool listeningSuppress = s_keybindListening;
        s_keybindListening = false;

        // Menu key — falls back to RSHIFT if user cleared the binding or
        // the value is out of the valid VK range, so the menu can never
        // become unreachable AND GetAsyncKeyState never sees a junk value.
        const bool menuValid = (g_settings.menuKey > 0 && g_settings.menuKey <= 0xFE);
        const int  menuVk    = menuValid ? g_settings.menuKey : VK_RSHIFT;
        const bool menuEdge  = (GetAsyncKeyState(menuVk) & 1) != 0;

        // ESC only closes the overlay (never opens). Suppress it while a
        // keybind picker is active so ESC stays usable as "cancel pick".
        const bool escEdge  = s_visible && !listeningSuppress &&
                              ((GetAsyncKeyState(VK_ESCAPE) & 1) != 0);

        if (menuEdge || escEdge) {
            const bool wasVisible = s_visible;
            s_visible = menuEdge ? !s_visible : false;
            ClipCursor(nullptr);
            ReleaseCapture();
            // Track exactly how many ShowCursor(TRUE) calls we pumped on
            // open so we can undo precisely on close. Without this the
            // counter takes seconds to drain back via GLFW's per-frame
            // ShowCursor(FALSE) (one decrement per frame), and the OS
            // cursor lingers visibly for ~50 frames after the menu hides.
            static int s_cursorPumpCount = 0;
            if (s_visible) {
                // Pump ShowCursor counter to 0+. GLFW slammed it deeply
                // negative pre-load; one ShowCursor(TRUE) bumps by 1.
                int safety = 256;
                int pumped = 0;
                while (safety-- > 0 && o_ShowCursor(TRUE) < 0) { pumped++; }
                s_cursorPumpCount = pumped;

                // Sync ImGui's MousePos to the actual OS cursor position
                // immediately. Without this, the first frame draws the
                // software cursor at last menu session's MousePos; the
                // moment the user nudges the mouse, ImGui_ImplWin32 polls
                // GetCursorPos and io.MousePos snaps to wherever GLFW was
                // centering the OS cursor (screen middle). Pre-syncing
                // here means the first-frame cursor matches the actual
                // mouse position, so no visible jump on first move.
                POINT op;
                if (GetCursorPos(&op) && s_hwnd) {
                    ScreenToClient(s_hwnd, &op);
                    ImGui::GetIO().AddMousePosEvent((float)op.x, (float)op.y);
                }

                if (o_SetCursor) {
                    o_SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)));
                }
            } else if (wasVisible) {
                // Undo exactly what we pumped on open. Each call decrements
                // by 1; after this loop the counter is back at the value
                // GLFW expects, so the OS cursor hides immediately instead
                // of lingering for a second while GLFW slowly drains it.
                while (s_cursorPumpCount > 0) {
                    o_ShowCursor(FALSE);
                    s_cursorPumpCount--;
                }
            }
            if (s_visible && !wasVisible) ReleaseAllHeldInputs();
            if (!s_visible && wasVisible) g_settings.Save();
            if (escEdge) s_eatEscUntilRelease = true;
        }

        // Drop the eater the moment ESC is physically released.
        if (s_eatEscUntilRelease && !(GetAsyncKeyState(VK_ESCAPE) & 0x8000))
            s_eatEscUntilRelease = false;

        // Module toggle keys. We edge-detect off the held bit (& 0x8000)
        // rather than the documented-as-unreliable "pressed since last call"
        // low bit. CapsLock in particular is queried constantly by other
        // system components for its lock-state indicator, which consumed
        // our edge before we could see it — the GUI checkbox followed any
        // toggle that did fire, but most key presses produced zero toggles
        // so the actual ESP draw state appeared "stuck".
        static bool s_espKeyHeldPrev      = false;
        static bool s_acKeyHeldPrev       = false;
        static bool s_aimKeyHeldPrev      = false;
        static bool s_destructKeyHeldPrev = false;

        const bool espHeld =
            (g_settings.espKey > 0 && g_settings.espKey <= 0xFE) &&
            (GetAsyncKeyState(g_settings.espKey) & 0x8000);
        const bool acHeld  =
            (g_settings.acKey > 0 && g_settings.acKey <= 0xFE) &&
            (GetAsyncKeyState(g_settings.acKey)  & 0x8000);
        const bool aimHeld =
            (g_settings.aimKey > 0 && g_settings.aimKey <= 0xFE) &&
            (GetAsyncKeyState(g_settings.aimKey) & 0x8000);
        const bool destructHeld =
            (g_settings.selfDestructKey > 0 && g_settings.selfDestructKey <= 0xFE) &&
            (GetAsyncKeyState(g_settings.selfDestructKey) & 0x8000);

        if (!listeningSuppress) {
            if (espHeld      && !s_espKeyHeldPrev)      g_settings.espEnabled  = !g_settings.espEnabled;
            if (acHeld       && !s_acKeyHeldPrev)       g_settings.acEnabled   = !g_settings.acEnabled;
            if (aimHeld      && !s_aimKeyHeldPrev)      g_settings.aimEnabled  = !g_settings.aimEnabled;
            // Self-destruct is one-way: edge-press flips selfDestruct true,
            // which the autoclicker loop polls + then drives `destruct = true`
            // for an orderly DLL unload. Same flag the menu's Self-Destruct
            // button sets, so there's exactly one path into the teardown.
            if (destructHeld && !s_destructKeyHeldPrev) g_settings.selfDestruct = true;
        }
        s_espKeyHeldPrev      = espHeld;
        s_acKeyHeldPrev       = acHeld;
        s_aimKeyHeldPrev      = aimHeld;
        s_destructKeyHeldPrev = destructHeld;
    }

    const bool needFrame = s_visible || g_settings.espEnabled;
    if (needFrame)
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();

        // Manual mouse-button polling. GLFW3 registers raw input with
        // RIDEV_NOLEGACY, which tells Windows NOT to synthesize the
        // legacy WM_MOUSEMOVE / WM_LBUTTONDOWN / WM_LBUTTONUP / etc.
        // messages — they simply never fire, so ImGui's WndProc-backed
        // mouse-button path receives nothing and clicks don't register
        // (hover still works because ImGui_ImplWin32_NewFrame polls
        // GetCursorPos for position). Poll GetAsyncKeyState and emit
        // edge events into ImGui's input queue ourselves. Only feed
        // events while the menu is visible so we don't accidentally
        // route the user's MC left-click attacks through ImGui.
        if (s_visible) {
            ImGuiIO& io = ImGui::GetIO();
            // Draw a software cursor — the OS cursor is unreliable while
            // MC has the window in disabled-cursor mode, even after our
            // ShowCursor pump (some GLFW configs / drivers blank the
            // cursor at a lower layer than ShowCursor can reach).
            io.MouseDrawCursor = true;

            static bool s_imPrevBtn[5] = {};
            const int  vks[5]   = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON,
                                    VK_XBUTTON1, VK_XBUTTON2 };
            for (int i = 0; i < 5; ++i) {
                const bool down = (GetAsyncKeyState(vks[i]) & 0x8000) != 0;
                if (down != s_imPrevBtn[i]) {
                    io.AddMouseButtonEvent(i, down);
                    s_imPrevBtn[i] = down;
                }
            }
        } else {
            ImGui::GetIO().MouseDrawCursor = false;
        }

        ImGui::NewFrame();

        const ImVec2 display = ImGui::GetIO().DisplaySize;

        if (g_settings.espEnabled)
            DrawEsp(display.x, display.y);

        if (s_visible)
        {
            // Translucent black backdrop behind the menu — reads as a modal
            // pause overlay (the game keeps rendering but is visibly dimmed).
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                ImVec2(0, 0), display, IM_COL32(0, 0, 0, 140));

            // Sidebar + content layout (ported from the reference GUI in
            // .temp/...): WindowPadding 0 so the navbar can render flush to
            // the left edge with its own rounded corner.
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::SetNextWindowPos (ImVec2(display.x * 0.5f, display.y * 0.5f),
                                     ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(580, 380), ImGuiCond_Always);
            ImGui::Begin("manuclicker", nullptr,
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize   |
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoMove);
            ImGui::PopStyleVar();

            const float   SIDEBAR_W = 150.0f;
            const ImVec2  winSize   = ImGui::GetWindowSize();
            const ImVec2  winPos    = ImGui::GetWindowPos();

            // ── Sidebar ──────────────────────────────────────────────────
            ImGui::BeginChild("##sidebar", ImVec2(SIDEBAR_W, winSize.y),
                false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
            {
                // Sidebar background + right divider drawn manually so the
                // rounded-left corner sits flush against the window's own
                // rounding.
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(winPos,
                    ImVec2(winPos.x + SIDEBAR_W, winPos.y + winSize.y),
                    ImGui::GetColorU32(ImGuiCol_ChildBg),
                    ImGui::GetStyle().WindowRounding,
                    ImDrawFlags_RoundCornersLeft);
                dl->AddRectFilled(
                    ImVec2(winPos.x + SIDEBAR_W - 1, winPos.y),
                    ImVec2(winPos.x + SIDEBAR_W,     winPos.y + winSize.y),
                    ImGui::GetColorU32(ImGuiCol_Border));

                ImGui::SetCursorPos(ImVec2(20, 22));
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
                ImGui::TextUnformatted("manuclicker");
                ImGui::PopFont();

                // Glyphs from Segoe Fluent Icons / MDL2 Assets — UTF-8 byte
                // escapes for codepoints in the Private Use Area:
                //   U+E1F5  Target (crosshair-ish)
                //   U+E716  People
                //   U+E713  Settings (gear)
                ImGui::SetCursorPos(ImVec2(0, 76));
                if (SidebarTab("Autoclicker", s_currentTab == 0)) s_currentTab = 0;
                if (SidebarTab("Aim",         s_currentTab == 1)) s_currentTab = 1;
                if (SidebarTab("ESP",         s_currentTab == 2)) s_currentTab = 2;
                if (SidebarTab("Friends",     s_currentTab == 3)) s_currentTab = 3;
                if (SidebarTab("Macros",      s_currentTab == 4)) s_currentTab = 4;
                if (SidebarTab("Clans",       s_currentTab == 5)) s_currentTab = 5;
                if (SidebarTab("Settings",    s_currentTab == 6)) s_currentTab = 6;
            }
            ImGui::EndChild();

            ImGui::SameLine(0, 0);

            // ── Content ──────────────────────────────────────────────────
            ImGui::BeginChild("##content", ImVec2(winSize.x - SIDEBAR_W, winSize.y),
                false, ImGuiWindowFlags_NoBackground);
            {
                // Big bold title in the top-left of the content area.
                ImGui::SetCursorPos(ImVec2(22, 22));
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
                ImGui::TextUnformatted(
                    s_currentTab == 0 ? "Autoclicker" :
                    s_currentTab == 1 ? "Aimassist"  :
                    s_currentTab == 2 ? "ESP"         :
                    s_currentTab == 3 ? "Friends"     :
                    s_currentTab == 4 ? "Macros"      :
                    s_currentTab == 5 ? "Clans"       :
                                        "Settings");
                ImGui::PopFont();

                // Body region. The Self-Destruct button lives inside the
                // Settings tab body now, so bodyBottom is just a small visual
                // margin to the window's bottom border. NoScrollbar — the
                // scrollbar is hidden entirely (mouse-wheel scroll still
                // works in tabs that overflow). With no scrollbar to leave
                // room for, the body uses the global symmetric WindowPadding
                // and sits with equal margins on both sides.
                const float bodyTop          = 64.0f;
                const float bodyBottom       = 20.0f;
                const float bodyRightMargin  = 22.0f;
                ImGui::SetCursorPos(ImVec2(22, bodyTop));
                ImGui::BeginChild("##body",
                    ImVec2(winSize.x - SIDEBAR_W - 22 - bodyRightMargin,
                           winSize.y - bodyTop - bodyBottom),
                    false,
                    ImGuiWindowFlags_NoBackground |
                    ImGuiWindowFlags_NoScrollbar);

                // Zero ItemSpacing so the rows' bottom borders chain into a
                // single continuous separator list (the reference look).
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

                // Any Row* widget returning true means a setting changed
                // this frame; flush to disk once at the end of the tab so
                // the persisted state survives any subsequent path that
                // doesn't get to call Save (DLL force-unload, game crash
                // with menu open, UNLOAD button, etc.).
                bool dirty = false;

                if (s_currentTab == 0)
                {
                    dirty |= RowCheckbox("Enabled",      &g_settings.acEnabled);
                    dirty |= RowCheckbox("Break Blocks", &g_settings.breakBlocks);
                    dirty |= RowSlider  ("CPS",          &g_settings.cps, 1, 50);
                    dirty |= RowCheckbox("Jitter",       &g_settings.jitterEnabled);
                    if (g_settings.jitterEnabled)
                        dirty |= RowSlider("Jitter Strength", &g_settings.jitterStrength, 0, 10);
                    dirty |= RowKeybind ("Toggle Key",   &g_settings.acKey);
                }
                else if (s_currentTab == 1)
                {
                    dirty |= RowCheckbox("Enabled",      &g_settings.aimEnabled);
                    dirty |= RowCheckbox("Click Assist", &g_settings.aimClickOnly);
                    dirty |= RowSlider  ("Horizontal Speed", &g_settings.aimSpeedH, 0, 10);
                    dirty |= RowSlider  ("Vertical Speed",   &g_settings.aimSpeedV, 0, 10);
                    dirty |= RowSlider  ("FOV (deg)",        &g_settings.aimFov,    1, 180);
                    dirty |= RowSlider  ("Range (blocks)",   &g_settings.aimRange,  1, 64);
                    dirty |= RowKeybind ("Toggle Key",       &g_settings.aimKey);
                }
                else if (s_currentTab == 2)
                {
                    dirty |= RowCheckbox("Enabled",  &g_settings.espEnabled);
                    if (g_settings.espEnabled) {
                        dirty |= RowCheckbox("Box",                &g_settings.drawBox);
                        dirty |= RowCheckbox("Name",               &g_settings.drawName);
                        dirty |= RowCheckbox("Distance",           &g_settings.drawDistance);
                        dirty |= RowCheckbox("Health",             &g_settings.drawHealth);
                        dirty |= RowCheckbox("Highlight Friends",  &g_settings.highlightFriends);
                    }
                    dirty |= RowKeybind("Toggle Key", &g_settings.espKey);
                }
                else if (s_currentTab == 3)
                {
                    // Friends tab — separate from ESP because the list can
                    // grow long and we want it scrollable without pushing
                    // the ESP toggles out of view. Toggle Friend Key lives
                    // here too so all friend-related controls cluster.
                    //
                    // Flip back to default ItemSpacing for this tab; the
                    // free-form list of names reads better with breathing
                    // room than chained into a continuous border list.
                    ImGui::PopStyleVar();

                    // Pass allowMouse=true so the user can bind MMB (the
                    // asking use-case: middle-click a player to friend
                    // them). The friends module already gates on
                    // Overlay::IsMenuVisible() and foreground-window so
                    // an MMB bind never double-fires inside the menu.
                    dirty |= RowKeybind("Bind", &g_settings.friendKey, true);

                    ImGui::Dummy(ImVec2(0, 6));

                    // ── Manual add row ──────────────────────────────────
                    // Buffer survives across frames so the user can type
                    // mid-edit without each frame nuking their input.
                    // Cleared after a successful add.
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
                        // Trim + lowercase. MC usernames are case-insensitive
                        // on the server, so the on-disk canonical form is
                        // lowercase to keep the ESP-side string compare cheap.
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

                    // ── List ────────────────────────────────────────────
                    // Snapshot under the mutex — the friends module could
                    // be mutating the underlying vector from its own
                    // thread, and iterating it raw would race with
                    // push_back's potential reallocation.
                    std::vector<std::string> snap;
                    {
                        std::lock_guard<std::mutex> lk(g_settings.friendsMutex);
                        snap = g_settings.friends;
                    }

                    if (!snap.empty()) {
                        // Deferred delete index — mutating the vector while
                        // iterating its snapshot would still leave the live
                        // list in sync, but a single deletion per frame is
                        // simpler to reason about than a remove-while-scan.
                        int toDelete = -1;
                        for (int i = 0; i < (int)snap.size(); ++i) {
                            ImGui::PushID(i);

                            // Right-aligned delete button. availX is captured
                            // BEFORE drawing the label so SameLine(availX - delW)
                            // can jump to the row's right edge regardless of
                            // label length — the earlier Dummy-based layout
                            // computed availX after the label, which yielded a
                            // bogus spacer width on long names.
                            const float delW   = 24.0f;
                            const float availX = ImGui::GetContentRegionAvail().x;

                            ImGui::AlignTextToFramePadding();
                            ImGui::TextUnformatted(snap[i].c_str());

                            ImGui::SameLine(availX - delW);
                            // Same red-on-hover styling as the macros tab
                            // delete button so the destructive intent reads
                            // consistently across tabs. Solid background
                            // (not the 0-alpha rest state used in macros) so
                            // the button is discoverable without hover —
                            // the user reported "no delete button" before.
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
                            // Re-find by name in the live vector — the
                            // snapshot index could be stale if the friends
                            // module appended between snap-copy and now.
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
                }
                else if (s_currentTab == 4)
                {
                    // Macros — flip back to default ItemSpacing for this tab.
                    // The other tabs intentionally use zero spacing so adjacent
                    // rows chain into a continuous separator list (reference
                    // look); a list of free-form macro entries reads better
                    // with breathing room between them.
                    ImGui::PopStyleVar();

                    // Defer the destructive ops until after the loop so the
                    // active iteration index isn't shifted under our feet.
                    int toDelete = -1;

                    for (int i = 0; i < g_settings.macroCount; ++i) {
                        ImGui::PushID(i);

                        // Header is the macro's item name so the user can
                        // scan the list visually instead of mapping "Macro 3"
                        // → "Golden Apple" in their head. Falls back until
                        // they type a name.
                        const char* hdr = g_settings.macros[i].name[0]
                            ? g_settings.macros[i].name
                            : "Unnamed Macro";
                        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
                        ImGui::TextUnformatted(hdr);
                        ImGui::PopFont();

                        // Right-aligned delete button. Plain text "x" instead
                        // of the previous Fluent-Icons trash glyph — the PUA
                        // icons read as tacky next to the otherwise-text UI.
                        // Width 44 fits "Delete" comfortably if we ever swap;
                        // for now the bare "x" keeps it visually light.
                        const float delW   = 24.0f;
                        const float availX = ImGui::GetContentRegionAvail().x;
                        ImGui::SameLine(availX - delW);
                        ImGui::PushStyleColor(ImGuiCol_Button,        FromHex(0x161d2e, 0.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, FromHex(0x9b1c1c, 0.6f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  FromHex(0x7a1414, 0.8f));
                        if (ImGui::Button("x##del", ImVec2(delW, 24)))
                            toDelete = i;
                        ImGui::PopStyleColor(3);

                        // Full-width name input. Saves on every keystroke so
                        // an in-progress edit isn't lost if the user closes
                        // the menu via UNLOAD / game crash.
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::InputTextWithHint("##name",
                                "item name (e.g. golden apple)",
                                g_settings.macros[i].name,
                                sizeof(g_settings.macros[i].name)))
                            dirty = true;

                        // Delay as a plain numeric input with +/- step buttons.
                        // Clamp on commit so a hand-typed 99999 can't reach
                        // the macros thread.
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

                    // Apply pending delete: shift down and clear the tail
                    // so a future re-add doesn't inherit stale state.
                    if (toDelete >= 0 && toDelete < g_settings.macroCount) {
                        for (int j = toDelete; j < g_settings.macroCount - 1; ++j)
                            g_settings.macros[j] = g_settings.macros[j + 1];
                        g_settings.macros[g_settings.macroCount - 1] = Macro{};
                        g_settings.macroCount--;
                        dirty = true;
                    }

                    // "+ Add Macro" button — only shown if there's room. The
                    // glyph is U+E710 (Segoe Fluent / MDL2 "Add").
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
                }
                else if (s_currentTab == 5)
                {
                    // EXPERIMENTAL banner. Drops out of zero-ItemSpacing for
                    // a few lines so the warning + section header get some
                    // breathing room above the chained-row settings below.
                    // This tab houses ability-exploit cheats targeting the
                    // Clans/Champions gamemode — currently auto-leap; more
                    // get added here as they're built.
                    ImGui::PopStyleVar(); // zero ItemSpacing
                    ImGui::PushStyleColor(ImGuiCol_Text, FromHex(0xf2c14e));
                    ImGui::TextUnformatted("EXPERIMENTAL");
                    ImGui::PopStyleColor();
                    ImGui::PushStyleColor(ImGuiCol_Text, FromHex(0x707a8c));
                    ImGui::TextWrapped(
                        "Ability exploits for the Clans/Champions");
                    ImGui::PopStyleColor();
                    ImGui::Dummy(ImVec2(0, 6));

                    // Per-ability section header. Adding a new exploit here
                    // means another bold label + its rows below. PushID per
                    // section so duplicate row labels ("Enabled", "Hold Key")
                    // don't collide on the ImGui widget IDs.
                    //
                    // ItemSpacing is pushed once below and held across both
                    // sections so the outer PopStyleVar at the end of the
                    // tab balances exactly one push. The inter-section gap
                    // uses an explicit Dummy(0, 6) rather than relying on
                    // spacing.
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

                    ImGui::PushID("leap");
                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
                    ImGui::TextUnformatted("Auto Leap");
                    ImGui::PopFont();
                    dirty |= RowCheckbox("Enabled",       &g_settings.leapEnabled);
                    dirty |= RowCheckbox("Require Axe",   &g_settings.leapRequireAxe);
                    dirty |= RowSlider  ("Interval (ms)", &g_settings.leapInterval, 50, 1000);
                    dirty |= RowKeybind ("Hold Key",      &g_settings.leapKey);
                    ImGui::PopID();

                    // Visual separator between the two ability cheats.
                    // Padding above + below so the line breathes; thin
                    // border-colored line so it reads as a divider, not
                    // another row's bottom edge.
                    ImGui::Dummy(ImVec2(0, 10));
                    {
                        const ImVec2 p = ImGui::GetCursorScreenPos();
                        const float  w = ImGui::GetContentRegionAvail().x;
                        ImGui::GetWindowDrawList()->AddLine(
                            p, ImVec2(p.x + w, p.y),
                            ImGui::GetColorU32(ImGuiCol_Border));
                    }
                    ImGui::Dummy(ImVec2(0, 10));

                    ImGui::PushID("autoability");
                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
                    ImGui::TextUnformatted("Auto Ability");
                    ImGui::PopFont();
                    dirty |= RowCheckbox("Enabled",        &g_settings.autoAbilityEnabled);
                    dirty |= RowCheckbox("Require Sword",  &g_settings.autoAbilityRequireSword);
                    dirty |= RowSlider  ("Delay (ms)",     &g_settings.autoAbilityDelay,    30, 1000);
                    // Cooldown UI is in whole seconds (0–30), but storage
                    // stays as milliseconds so the module's chrono-ms
                    // comparison doesn't need a unit conversion. Truncate-
                    // to-second on display, multiply-by-1000 on commit;
                    // the small reading↔storage round-trip mismatch only
                    // matters on first show of a legacy config (e.g.
                    // 600ms saved → slider reads 0s — user just nudges).
                    {
                        int cooldownSec = g_settings.autoAbilityCooldown / 1000;
                        if (cooldownSec > 30) cooldownSec = 30;
                        if (cooldownSec < 0)  cooldownSec = 0;
                        if (RowSlider("Cooldown (s)", &cooldownSec, 0, 30, "%ds")) {
                            g_settings.autoAbilityCooldown = cooldownSec * 1000;
                            dirty = true;
                        }
                    }
                    ImGui::PopID();
                }
                else if (s_currentTab == 6)
                {
                    dirty |= RowKeybind("Menu Key",          &g_settings.menuKey);
                    dirty |= RowKeybind("Self-Destruct Key", &g_settings.selfDestructKey);

                    EspModule::Snapshot snap;
                    {
                        std::lock_guard<std::mutex> lk(EspModule::snapMutex);
                        snap = EspModule::snapshot;
                    }
                    ImGui::PushStyleColor(ImGuiCol_Text, FromHex(0x707a8c));
                    ImGui::Text("valid=%d  mc=%d  lp=%d  lvl=%d  gr=%d  cam=%d",
                        snap.valid, snap.gotMinecraft, snap.gotLocalPlayer,
                        snap.gotLevel, snap.gotGameRenderer, snap.gotCamera);
                    ImGui::Text("players()=%d  targets=%d",
                        snap.rawPlayerCount, (int)snap.targets.size());
                    ImGui::Text("cam=(%.1f,%.1f,%.1f)  yaw=%.1f  pitch=%.1f  fov=%.1f",
                        snap.cam.x, snap.cam.y, snap.cam.z,
                        snap.cam.yRot, snap.cam.xRot, snap.cam.fov);
                    ImGui::PopStyleColor();

                    // Self-Destruct button — moved inside the Settings tab so
                    // it can't be triggered accidentally from any tab via a
                    // misclick on a permanent footer. Same flag as the
                    // selfDestructKey edge-detect path; the autoclicker thread
                    // polls it + drives the orderly DLL teardown.
                    ImGui::PopStyleVar(); // zero ItemSpacing
                    ImGui::Dummy(ImVec2(0, 8));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                    ImGui::PushStyleColor(ImGuiCol_Button,        FromHex(0x9b1c1c));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, FromHex(0xb91c1c));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  FromHex(0x7a1414));
                    ImGui::PushStyleColor(ImGuiCol_Text,          FromHex(0xffffff));
                    if (ImGui::Button("Self-Destruct",
                            ImVec2(ImGui::GetContentRegionAvail().x, 32)))
                        g_settings.selfDestruct = true;
                    ImGui::PopStyleColor(4);
                    ImGui::PopStyleVar();
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                }

                if (dirty) g_settings.Save();

                ImGui::PopStyleVar(); // ItemSpacing

                ImGui::EndChild(); // ##body
            }
            ImGui::EndChild(); // ##content

            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    return o_wglSwapBuffers(hdc);
}

namespace Overlay
{
    bool IsMenuVisible() { return s_visible; }

    void Init()
    {
        MH_Initialize();

        void* tgtSwap = reinterpret_cast<void*>(
            GetProcAddress(GetModuleHandleA("opengl32.dll"), "wglSwapBuffers"));
        MH_CreateHook(tgtSwap, reinterpret_cast<void*>(hk_wglSwapBuffers),
                      reinterpret_cast<void**>(&o_wglSwapBuffers));
        MH_EnableHook(tgtSwap);

        // Hook the user32 calls GLFW uses to keep the cursor centered in
        // disabled-cursor mode (cursor-snap-back jitter) PLUS the raw-input
        // read functions (gates mouse delta delivery to MC when our menu is
        // open — no registration tampering required). GetRawInputBuffer is
        // the polling path; GetRawInputData is the WM_INPUT processing path.
        // Both observe s_visible and short-circuit while the menu is up.
        HMODULE u32 = GetModuleHandleA("user32.dll");
        if (u32) {
            struct { const char* name; void* hook; void** orig; } hooks[] = {
                { "SetCursorPos",       reinterpret_cast<void*>(hk_SetCursorPos),       reinterpret_cast<void**>(&o_SetCursorPos)       },
                { "ClipCursor",         reinterpret_cast<void*>(hk_ClipCursor),         reinterpret_cast<void**>(&o_ClipCursor)         },
                { "ShowCursor",         reinterpret_cast<void*>(hk_ShowCursor),         reinterpret_cast<void**>(&o_ShowCursor)         },
                { "SetCursor",          reinterpret_cast<void*>(hk_SetCursor),          reinterpret_cast<void**>(&o_SetCursor)          },
                { "GetRawInputBuffer",  reinterpret_cast<void*>(hk_GetRawInputBuffer),  reinterpret_cast<void**>(&o_GetRawInputBuffer)  },
                { "GetRawInputData",    reinterpret_cast<void*>(hk_GetRawInputData),    reinterpret_cast<void**>(&o_GetRawInputData)    },
            };
            for (auto& h : hooks) {
                void* target = reinterpret_cast<void*>(GetProcAddress(u32, h.name));
                if (target && MH_CreateHook(target, h.hook, h.orig) == MH_OK)
                    MH_EnableHook(target);
            }
        }
    }

    void Shutdown()
    {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();

        if (s_initialized)
        {
            if (s_hwnd && s_origProc)
                SetWindowLongPtrW(s_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(s_origProc));
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
    }
}
