#include "Overlay.h"
#include <Windows.h>
#include <gl/GL.h>
#include <cmath>
#include <mutex>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
#include "../Settings.h"
#include "../modules/esp/EspModule.h"
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
static int     s_currentTab         = 0; // 0=Autoclicker, 1=ESP, 2=Macros, 3=Aim, 4=Settings
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
static bool RowKeybind(const char* label, int* vk)
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

    ImGuiStorage* storage = &window->StateStorage;
    bool changed = false;

    // Single state int instead of the two-bool coordination we had before.
    // The old version stored `listening` at `id` and `armed` at `id+1`; if
    // one half lagged or the storage at `id+1` collided with anything, the
    // picker could enter listening mode but never advance to the accepting
    // state (the symptom the user hit: pill stuck on "press a key").
    // One int, three states, no coordination problem.
    enum { IDLE = 0, ARMING = 1, ACCEPTING = 2 };
    int state = storage->GetInt(id, IDLE);

    if (pressed) state = ARMING;

    // Right-click on the pill clears the binding.
    if (IsMouseHoveringRect(pill.Min, pill.Max) &&
        IsMouseClicked(ImGuiMouseButton_Right)) {
        if (*vk != 0) { *vk = 0; changed = true; }
        state = IDLE;
    }

    auto isFilteredVk = [](int k) {
        // Mouse buttons aren't bindable here (they'd conflict with clicking
        // around the menu). Menu key is also filtered while arming so opening
        // the overlay via e.g. INSERT can't leave the arming scan thinking a
        // key is still held forever.
        return k == VK_LBUTTON || k == VK_RBUTTON || k == VK_MBUTTON ||
               k == VK_XBUTTON1 || k == VK_XBUTTON2;
    };

    if (state != IDLE) {
        s_keybindListening = true;

        const int menuVk = (g_settings.menuKey > 0 && g_settings.menuKey <= 0xFE)
            ? g_settings.menuKey : VK_INSERT;

        if (state == ARMING) {
            // Wait for everything to be released. We skip the menu key
            // explicitly — if the user opened the overlay via INSERT and
            // still has it physically held, that's not a key they're trying
            // to bind here. Without the skip the picker would stall until
            // they release INSERT, which they may not realize they're holding.
            bool anyHeld = false;
            for (int k = 0x07; k <= 0xFE; ++k) {
                if (isFilteredVk(k)) continue;
                if (k == menuVk)     continue;
                if (GetAsyncKeyState(k) & 0x8000) { anyHeld = true; break; }
            }
            if (!anyHeld) state = ACCEPTING;
        }
        else if (state == ACCEPTING) {
            // First held key wins. We don't skip the menu key here — once
            // armed, the user is actively trying to bind something, and if
            // they want to re-bind to the menu key that should work.
            for (int k = 0x07; k <= 0xFE; ++k) {
                if (isFilteredVk(k)) continue;
                if (!(GetAsyncKeyState(k) & 0x8000)) continue;

                if (k == VK_ESCAPE) {
                    // ESC clears the binding back to none.
                    if (*vk != 0) { *vk = 0; changed = true; }
                } else {
                    *vk     = k;
                    changed = true;
                }
                state = IDLE;
                break;
            }
        }
    }

    storage->SetInt(id, state);
    const bool listening = (state != IDLE);

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

// Full-width clickable row used as a sidebar tab item: an icon glyph on the
// left, a label to its right, and a 2-px full-height accent stripe along the
// right edge that slides between tabs when the selection changes. Mirrors
// the `custom::tab` style from the reference GUI in .temp/.../custom.cpp.
//
// The accent stripe uses a static `line_pos` shared across every tab call —
// only the selected tab updates the target, all tabs paint at the same Y so
// the stripe visually "moves" between them as line_pos lerps.
static bool SidebarTab(const char* icon, const char* label, bool selected)
{
    using namespace ImGui;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImVec2 p = window->DC.CursorPos;
    ImVec2 size(window->Size.x, 38.0f);
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

    // Icon glyph from the MDL2 / Fluent Icons font in Fonts[1].
    PushFont(GetIO().Fonts->Fonts[1]);
    ImVec2 iconSz = CalcTextSize(icon);
    RenderText(ImVec2(bb.Min.x + 20.0f - iconSz.x * 0.5f,
                      bb.GetCenter().y - iconSz.y * 0.5f), icon);
    PopFont();

    ImVec2 labelSz = CalcTextSize(label);
    RenderText(ImVec2(bb.Min.x + 42.0f,
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

static void DrawEsp(float dispW, float dispH)
{
    EspModule::Snapshot snap;
    {
        std::lock_guard<std::mutex> lk(EspModule::snapMutex);
        if (!EspModule::snapshot.valid) return;
        snap = EspModule::snapshot;
    }

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

        float minSX =  1e9f, minSY =  1e9f;
        float maxSX = -1e9f, maxSY = -1e9f;
        bool any = false;

        for (int i = 0; i < 8; ++i)
        {
            ImVec2 p;
            double wx = cx[(i >> 0) & 1];
            double wy = cy[(i >> 1) & 1];
            double wz = cz[(i >> 2) & 1];
            if (!ProjectWorld(wx, wy, wz, snap.cam, dispW, dispH, p)) continue;
            any = true;
            if (p.x < minSX) minSX = p.x;
            if (p.y < minSY) minSY = p.y;
            if (p.x > maxSX) maxSX = p.x;
            if (p.y > maxSY) maxSY = p.y;
        }
        if (!any) continue;

        // Clip insane offscreen values
        if (maxSX < 0 || minSX > dispW || maxSY < 0 || minSY > dispH) continue;

        if (g_settings.drawBox)
        {
            // Drop alpha to ~220/255 so the box reads as a subtle outline
            // rather than fully saturated, matching the old hardcoded look
            // but in the player's team color.
            const ImU32 colBox = (t.boxColor & 0x00FFFFFFu) | (220u << 24);
            dl->AddRect(ImVec2(minSX, minSY), ImVec2(maxSX, maxSY), colBox, 0.0f, 0, 1.5f);
        }

        if (g_settings.drawName || g_settings.drawDistance)
        {
            // Build the list of colored chunks we'll render. drawName toggles
            // include the team-formatted player chunks; drawDistance appends
            // a final dimmed chunk with the distance.
            struct Chunk { std::string text; ImU32 col; };
            std::vector<Chunk> chunks;
            if (g_settings.drawName) {
                for (const auto& nc : t.nameChunks)
                    if (!nc.first.empty())
                        chunks.push_back({nc.first, (ImU32)nc.second});
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
    // GLFW also calls SetCursor(NULL) to blank the cursor image. Refuse —
    // keep whatever cursor is currently set so the arrow stays visible.
    if (s_visible && cursor == nullptr) return ::GetCursor();
    return o_SetCursor(cursor);
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

    {
        // Snapshot the keybind-listening flag from the previous frame and
        // reset it; this frame's RowKeybind will set it again if any
        // picker is still listening.
        const bool listeningSuppress = s_keybindListening;
        s_keybindListening = false;

        // Menu key — falls back to INSERT if user cleared the binding or
        // the value is out of the valid VK range, so the menu can never
        // become unreachable AND GetAsyncKeyState never sees a junk value.
        const bool menuValid = (g_settings.menuKey > 0 && g_settings.menuKey <= 0xFE);
        const int  menuVk    = menuValid ? g_settings.menuKey : VK_INSERT;
        const bool menuEdge  = (GetAsyncKeyState(menuVk) & 1) != 0;

        // ESC only closes the overlay (never opens). Suppress it while a
        // keybind picker is active so ESC stays usable as "cancel pick".
        const bool escEdge  = s_visible && !listeningSuppress &&
                              ((GetAsyncKeyState(VK_ESCAPE) & 1) != 0);

        if (menuEdge || escEdge) {
            const bool wasVisible = s_visible;
            s_visible = menuEdge ? !s_visible : false;
            ClipCursor(nullptr);
            ShowCursor(s_visible ? TRUE : FALSE);
            if (s_visible && !wasVisible) ReleaseAllHeldInputs();
            // Persist settings every time the user closes the overlay.
            if (!s_visible && wasVisible) g_settings.Save();
            // Arm the ESC-eater so the residual key state can't reach MC.
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
        static bool s_espKeyHeldPrev = false;
        static bool s_acKeyHeldPrev  = false;
        static bool s_aimKeyHeldPrev = false;

        const bool espHeld =
            (g_settings.espKey > 0 && g_settings.espKey <= 0xFE) &&
            (GetAsyncKeyState(g_settings.espKey) & 0x8000);
        const bool acHeld  =
            (g_settings.acKey > 0 && g_settings.acKey <= 0xFE) &&
            (GetAsyncKeyState(g_settings.acKey)  & 0x8000);
        const bool aimHeld =
            (g_settings.aimKey > 0 && g_settings.aimKey <= 0xFE) &&
            (GetAsyncKeyState(g_settings.aimKey) & 0x8000);

        if (!listeningSuppress) {
            if (espHeld && !s_espKeyHeldPrev) g_settings.espEnabled = !g_settings.espEnabled;
            if (acHeld  && !s_acKeyHeldPrev)  g_settings.acEnabled  = !g_settings.acEnabled;
            if (aimHeld && !s_aimKeyHeldPrev) g_settings.aimEnabled = !g_settings.aimEnabled;
        }
        s_espKeyHeldPrev = espHeld;
        s_acKeyHeldPrev  = acHeld;
        s_aimKeyHeldPrev = aimHeld;
    }

    const bool needFrame = s_visible || g_settings.espEnabled;
    if (needFrame)
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
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
                // Glyphs from Segoe Fluent Icons / MDL2 Assets — UTF-8 byte
                // escapes for codepoints in the Private Use Area:
                //   U+E1F5  Target (crosshair-ish, autoclicker)
                //   U+E716  People (ESP)
                //   U+E945  LightningBolt (macros)
                //   U+E18B  Bullseye (aim assist)
                //   U+E713  Settings (gear)
                ImGui::SetCursorPos(ImVec2(0, 76));
                if (SidebarTab("\xEE\x87\xB5", "Autoclicker", s_currentTab == 0)) s_currentTab = 0;
                if (SidebarTab("\xEE\x9C\x96", "ESP",         s_currentTab == 1)) s_currentTab = 1;
                if (SidebarTab("\xEE\xA5\x85", "Macros",      s_currentTab == 2)) s_currentTab = 2;
                if (SidebarTab("\xEE\x86\x8B", "Aim",         s_currentTab == 3)) s_currentTab = 3;
                if (SidebarTab("\xEE\x9C\x93", "Settings",    s_currentTab == 4)) s_currentTab = 4;
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
                    s_currentTab == 0 ? "autoclicker" :
                    s_currentTab == 1 ? "esp"         :
                    s_currentTab == 2 ? "macros"      :
                    s_currentTab == 3 ? "aim assist"  :
                                        "settings");
                ImGui::PopFont();

                // Body region — leaves room for the unload button anchored
                // at the bottom (height ≈ 32 + padding).
                const float bodyTop    = 64.0f;
                const float bodyBottom = 60.0f;
                ImGui::SetCursorPos(ImVec2(22, bodyTop));
                ImGui::BeginChild("##body",
                    ImVec2(winSize.x - SIDEBAR_W - 44, winSize.y - bodyTop - bodyBottom),
                    false, ImGuiWindowFlags_NoBackground);

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
                    dirty |= RowKeybind ("Toggle Key",   &g_settings.acKey);
                }
                else if (s_currentTab == 1)
                {
                    dirty |= RowCheckbox("Enabled",  &g_settings.espEnabled);
                    if (g_settings.espEnabled) {
                        dirty |= RowCheckbox("Box",      &g_settings.drawBox);
                        dirty |= RowCheckbox("Name",     &g_settings.drawName);
                        dirty |= RowCheckbox("Distance", &g_settings.drawDistance);
                    }
                    dirty |= RowKeybind("Toggle Key", &g_settings.espKey);
                }
                else if (s_currentTab == 2)
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

                        // Trash icon on the right side of the header row.
                        // U+E74D = Delete (Segoe Fluent / MDL2 Assets). The
                        // 32px offset accounts for the 24px button + a small
                        // margin so it never clips the scrollbar when the
                        // list overflows.
                        const float availX = ImGui::GetContentRegionAvail().x;
                        ImGui::SameLine(availX - 32.0f);
                        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
                        ImGui::PushStyleColor(ImGuiCol_Button,        FromHex(0x161d2e, 0.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, FromHex(0x9b1c1c, 0.6f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  FromHex(0x7a1414, 0.8f));
                        if (ImGui::Button("\xEE\x9D\x8D##del", ImVec2(24, 24)))
                            toDelete = i;
                        ImGui::PopStyleColor(3);
                        ImGui::PopFont();

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
                else if (s_currentTab == 3)
                {
                    dirty |= RowCheckbox("Enabled",      &g_settings.aimEnabled);
                    dirty |= RowCheckbox("Click Assist", &g_settings.aimClickOnly);
                    dirty |= RowSlider  ("Horizontal Speed", &g_settings.aimSpeedH, 0, 10);
                    dirty |= RowSlider  ("Vertical Speed",   &g_settings.aimSpeedV, 0, 10);
                    dirty |= RowSlider  ("FOV (deg)",        &g_settings.aimFov,    1, 180);
                    dirty |= RowSlider  ("Range (blocks)",   &g_settings.aimRange,  1, 64);

                    // Target part: feet / body center / head. Default Combo
                    // suffices — three options, no need for a styled widget.
                    // Pop spacing so the combo doesn't collapse against the
                    // neighbouring rows' bottom borders.
                    ImGui::PopStyleVar();
                    static const char* parts[] = { "Feet", "Body", "Head" };
                    if (ImGui::Combo("Target Part", &g_settings.aimTargetPart,
                                     parts, IM_ARRAYSIZE(parts)))
                        dirty = true;
                    ImGui::Dummy(ImVec2(0, 4));
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

                    dirty |= RowKeybind("Toggle Key", &g_settings.aimKey);
                }
                else
                {
                    dirty |= RowKeybind("Menu Key", &g_settings.menuKey);

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
                }

                if (dirty) g_settings.Save();

                ImGui::PopStyleVar(); // ItemSpacing

                ImGui::EndChild(); // ##body

                // Unload button anchored to the bottom-right of content.
                ImGui::SetCursorPos(ImVec2(22, winSize.y - 48));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                ImGui::PushStyleColor(ImGuiCol_Button,        FromHex(0x9b1c1c));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, FromHex(0xb91c1c));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  FromHex(0x7a1414));
                ImGui::PushStyleColor(ImGuiCol_Text,          FromHex(0xffffff));
                if (ImGui::Button("UNLOAD",
                        ImVec2(winSize.x - SIDEBAR_W - 44, 32)))
                    g_settings.selfDestruct = true;
                ImGui::PopStyleColor(4);
                ImGui::PopStyleVar();
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

        // Hook the two user32 calls GLFW uses to keep the cursor centered
        // in disabled-cursor mode. While the overlay is up, both no-op —
        // killing the cursor-snap-back jitter.
        HMODULE u32 = GetModuleHandleA("user32.dll");
        if (u32) {
            struct { const char* name; void* hook; void** orig; } hooks[] = {
                { "SetCursorPos", reinterpret_cast<void*>(hk_SetCursorPos), reinterpret_cast<void**>(&o_SetCursorPos) },
                { "ClipCursor",   reinterpret_cast<void*>(hk_ClipCursor),   reinterpret_cast<void**>(&o_ClipCursor)   },
                { "ShowCursor",   reinterpret_cast<void*>(hk_ShowCursor),   reinterpret_cast<void**>(&o_ShowCursor)   },
                { "SetCursor",    reinterpret_cast<void*>(hk_SetCursor),    reinterpret_cast<void**>(&o_SetCursor)    },
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
