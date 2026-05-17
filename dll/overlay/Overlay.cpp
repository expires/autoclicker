#include "Overlay.h"
#include <Windows.h>
#include <gl/GL.h>
#include <cmath>
#include <mutex>
#include "imgui.h"
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

static bool    s_initialized     = false;
static bool    s_visible         = false;
static bool    s_debugTabVisible = false;
static HWND    s_hwnd            = nullptr;
static WNDPROC s_origProc        = nullptr;

static ImVec4 FromHex(uint32_t hex, float alpha = 1.0f)
{
    return ImVec4(
        ((hex >> 16) & 0xFF) / 255.0f,
        ((hex >>  8) & 0xFF) / 255.0f,
        ( hex        & 0xFF) / 255.0f,
        alpha);
}

// Indigo title-case label + thin accent separator. Used to break each tab
// into visually grouped sections.
static void SectionHeader(const char* label)
{
    ImGui::Dummy({0, 4});
    ImGui::PushStyleColor(ImGuiCol_Text, FromHex(0x5c7cfa));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Separator, FromHex(0x5c7cfa, 0.30f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Dummy({0, 2});
}

// Checkbox where "checked" = the whole box filled with the accent color, not
// a checkmark icon. ImGui's built-in Checkbox always uses FrameBg for the box
// + CheckMark on top; we swap FrameBg based on state and hide the checkmark.
static bool FilledCheckbox(const char* label, bool* v)
{
    const ImVec4 frameOff    = FromHex(0x161d2e);
    const ImVec4 frameOffHov = FromHex(0x1d2438);
    const ImVec4 frameOn     = FromHex(0x5865f2);
    const ImVec4 frameOnHov  = FromHex(0x6b76f3);
    const bool   on = *v;
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        on ? frameOn    : frameOff);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, on ? frameOnHov : frameOffHov);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  on ? frameOn    : frameOffHov);
    ImGui::PushStyleColor(ImGuiCol_CheckMark,      ImVec4(0, 0, 0, 0)); // hidden
    const bool changed = ImGui::Checkbox(label, v);
    ImGui::PopStyleColor(4);
    return changed;
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
            const float rounding = (bgBottom - bgTop) * 0.5f;

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

static LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (s_visible && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;
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

        char winDir[MAX_PATH];
        GetWindowsDirectoryA(winDir, MAX_PATH);

        char fontBold[MAX_PATH];
        char fontRegular[MAX_PATH];
        strcat_s(fontBold,    sizeof(fontBold),    winDir); strcat_s(fontBold,    sizeof(fontBold),    "\\Fonts\\segoeuib.ttf");
        strcat_s(fontRegular, sizeof(fontRegular), winDir); strcat_s(fontRegular, sizeof(fontRegular), "\\Fonts\\segoeui.ttf");

        ImFontConfig cfg;
        cfg.OversampleH = 3;
        cfg.OversampleV = 3;
        cfg.PixelSnapH  = false;

        const char* fontPath = (GetFileAttributesA(fontBold) != INVALID_FILE_ATTRIBUTES)
            ? fontBold : fontRegular;
        io.Fonts->AddFontFromFileTTF(fontPath, 16.0f, &cfg);

        ApplyStyle();

        ImGui_ImplWin32_Init(s_hwnd);
        ImGui_ImplOpenGL3_Init(nullptr);

        s_origProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(s_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc)));

        s_initialized = true;
    }

    if (GetAsyncKeyState(VK_INSERT) & 1)
    {
        s_visible = !s_visible;
        ClipCursor(nullptr);
        ShowCursor(s_visible ? TRUE : FALSE);
    }
    if (GetAsyncKeyState(VK_PRIOR)   & 1) s_debugTabVisible    = !s_debugTabVisible;
    if (GetAsyncKeyState(VK_CAPITAL) & 1) g_settings.espEnabled = !g_settings.espEnabled;

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
            ImGui::SetNextWindowSize({display.x * 0.30f, 0}, ImGuiCond_Always);
            ImGui::Begin("manuclicker", nullptr,
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoCollapse  |
                ImGuiWindowFlags_NoResize);

            if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_NoTooltip))
            {
                if (ImGui::BeginTabItem("Autoclicker"))
                {
                    SectionHeader("BEHAVIOR");
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 5.0f));
                    FilledCheckbox("Enabled",      &g_settings.acEnabled);
                    FilledCheckbox("Break Blocks", &g_settings.breakBlocks);
                    ImGui::PopStyleVar();

                    SectionHeader("CLICK RATE");
                    ImGui::TextUnformatted("CPS");
                    {
                        const float valueW = 28.0f;
                        const float availW = ImGui::GetContentRegionAvail().x;
                        ImGui::SetNextItemWidth(availW - valueW - 4.0f);
                        ImGui::SliderInt("##cps", &g_settings.cps, 1, 50, "");
                        ImGui::SameLine();
                        ImGui::Text("%d", g_settings.cps);
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("ESP"))
                {
                    SectionHeader("VISIBILITY");
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 5.0f));
                    FilledCheckbox("Enabled", &g_settings.espEnabled);
                    ImGui::PopStyleVar();

                    if (g_settings.espEnabled)
                    {
                        ImGui::Indent(16.0f);
                        ImGui::PushStyleColor(ImGuiCol_Text, FromHex(0x8892a4));
                        FilledCheckbox("Box",      &g_settings.drawBox);
                        FilledCheckbox("Name",     &g_settings.drawName);
                        FilledCheckbox("Distance", &g_settings.drawDistance);
                        ImGui::PopStyleColor();
                        ImGui::Unindent(16.0f);
                    }

                    ImGui::EndTabItem();
                }

                if (s_debugTabVisible && ImGui::BeginTabItem("Debug"))
                {
                    // Amber section label (per design) instead of the indigo
                    // accent — diagnostics are a different visual register.
                    ImGui::Dummy({0, 4});
                    ImGui::PushStyleColor(ImGuiCol_Text, FromHex(0xe8a020));
                    ImGui::TextUnformatted("DIAGNOSTICS");
                    ImGui::PopStyleColor();
                    ImGui::PushStyleColor(ImGuiCol_Separator, FromHex(0xe8a020, 0.30f));
                    ImGui::Separator();
                    ImGui::PopStyleColor();
                    ImGui::Dummy({0, 2});

                    EspModule::Snapshot snap;
                    {
                        std::lock_guard<std::mutex> lk(EspModule::snapMutex);
                        snap = EspModule::snapshot;
                    }

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, FromHex(0x0d1119));
                    ImGui::PushStyleColor(ImGuiCol_Border,  FromHex(0x1e2535));
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
                    ImGui::BeginChild("##diagblock",
                        ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 3.6f),
                        ImGuiChildFlags_Border);

                    ImGui::PushStyleColor(ImGuiCol_Text, FromHex(0x5a8a6a));
                    ImGui::Text("valid=%d  mc=%d  lp=%d  lvl=%d  gr=%d  cam=%d",
                        snap.valid, snap.gotMinecraft, snap.gotLocalPlayer,
                        snap.gotLevel, snap.gotGameRenderer, snap.gotCamera);
                    ImGui::Text("players()=%d  targets=%d",
                        snap.rawPlayerCount, (int)snap.targets.size());
                    ImGui::Text("cam=(%.1f,%.1f,%.1f)  yaw=%.1f  pitch=%.1f  fov=%.1f",
                        snap.cam.x, snap.cam.y, snap.cam.z,
                        snap.cam.yRot, snap.cam.xRot, snap.cam.fov);
                    ImGui::PopStyleColor();

                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor(2);

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            // Always-visible unload button at the bottom of the panel.
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,        FromHex(0x9b1c1c));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, FromHex(0xb91c1c));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  FromHex(0x7a1414));
            ImGui::PushStyleColor(ImGuiCol_Text,          FromHex(0xffffff));
            if (ImGui::Button("UNLOAD", {-1, 32}))
                g_settings.selfDestruct = true;
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();

            ImGui::Spacing();
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    return o_wglSwapBuffers(hdc);
}

namespace Overlay
{
    void Init()
    {
        MH_Initialize();
        void* target = reinterpret_cast<void*>(
            GetProcAddress(GetModuleHandleA("opengl32.dll"), "wglSwapBuffers"));
        MH_CreateHook(target, reinterpret_cast<void*>(hk_wglSwapBuffers),
                      reinterpret_cast<void**>(&o_wglSwapBuffers));
        MH_EnableHook(target);
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
