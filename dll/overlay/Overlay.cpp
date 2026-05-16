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
#include "../SDK/JvmtiAgent.h"
#include <MinHook.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Declared manually — intentionally commented out in imgui_impl_win32.h to avoid <windows.h> dependency
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef BOOL(WINAPI* fn_wglSwapBuffers)(HDC);
static fn_wglSwapBuffers o_wglSwapBuffers = nullptr;

static bool    s_initialized = false;
static bool    s_visible     = false;
static HWND    s_hwnd        = nullptr;
static WNDPROC s_origProc    = nullptr;

static void ApplyStyle()
{
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowRounding     = 8.0f;
    s.FrameRounding      = 5.0f;
    s.GrabRounding       = 5.0f;
    s.ScrollbarRounding  = 5.0f;
    s.TabRounding        = 5.0f;
    s.ChildRounding      = 5.0f;
    s.PopupRounding      = 5.0f;

    s.WindowPadding      = {14.0f, 12.0f};
    s.FramePadding       = {10.0f,  5.0f};
    s.ItemSpacing        = { 8.0f,  7.0f};
    s.ItemInnerSpacing   = { 6.0f,  4.0f};
    s.WindowBorderSize   = 1.0f;
    s.FrameBorderSize    = 0.0f;
    s.WindowMinSize      = {200.0f, 50.0f};

    ImVec4* c = s.Colors;

    // Backgrounds
    c[ImGuiCol_WindowBg]             = {0.05f, 0.06f, 0.09f, 1.00f};
    c[ImGuiCol_PopupBg]              = {0.07f, 0.09f, 0.12f, 1.00f};
    c[ImGuiCol_ChildBg]              = {0.07f, 0.09f, 0.12f, 1.00f};

    // Title
    c[ImGuiCol_TitleBg]              = {0.07f, 0.09f, 0.13f, 1.00f};
    c[ImGuiCol_TitleBgActive]        = {0.09f, 0.12f, 0.19f, 1.00f};
    c[ImGuiCol_TitleBgCollapsed]     = {0.05f, 0.06f, 0.09f, 1.00f};

    // Borders
    c[ImGuiCol_Border]               = {0.16f, 0.21f, 0.31f, 1.00f};
    c[ImGuiCol_BorderShadow]         = {0.00f, 0.00f, 0.00f, 0.00f};
    c[ImGuiCol_Separator]            = {0.16f, 0.21f, 0.31f, 1.00f};
    c[ImGuiCol_SeparatorHovered]     = {0.24f, 0.51f, 0.96f, 0.60f};
    c[ImGuiCol_SeparatorActive]      = {0.24f, 0.51f, 0.96f, 1.00f};

    // Frames
    c[ImGuiCol_FrameBg]              = {0.11f, 0.14f, 0.20f, 1.00f};
    c[ImGuiCol_FrameBgHovered]       = {0.15f, 0.20f, 0.29f, 1.00f};
    c[ImGuiCol_FrameBgActive]        = {0.19f, 0.25f, 0.37f, 1.00f};

    // Buttons (blue accent)
    c[ImGuiCol_Button]               = {0.11f, 0.31f, 0.78f, 1.00f};
    c[ImGuiCol_ButtonHovered]        = {0.16f, 0.40f, 0.94f, 1.00f};
    c[ImGuiCol_ButtonActive]         = {0.08f, 0.24f, 0.62f, 1.00f};

    // Sliders / checkmarks
    c[ImGuiCol_SliderGrab]           = {0.24f, 0.52f, 0.96f, 1.00f};
    c[ImGuiCol_SliderGrabActive]     = {0.36f, 0.62f, 1.00f, 1.00f};
    c[ImGuiCol_CheckMark]            = {0.36f, 0.62f, 1.00f, 1.00f};

    // Scrollbar
    c[ImGuiCol_ScrollbarBg]          = {0.05f, 0.06f, 0.09f, 1.00f};
    c[ImGuiCol_ScrollbarGrab]        = {0.16f, 0.21f, 0.31f, 1.00f};
    c[ImGuiCol_ScrollbarGrabHovered] = {0.24f, 0.52f, 0.96f, 0.60f};
    c[ImGuiCol_ScrollbarGrabActive]  = {0.24f, 0.52f, 0.96f, 1.00f};

    // Headers
    c[ImGuiCol_Header]               = {0.11f, 0.31f, 0.78f, 0.40f};
    c[ImGuiCol_HeaderHovered]        = {0.11f, 0.31f, 0.78f, 0.60f};
    c[ImGuiCol_HeaderActive]         = {0.11f, 0.31f, 0.78f, 0.80f};

    // Resize grip
    c[ImGuiCol_ResizeGrip]           = {0.24f, 0.52f, 0.96f, 0.15f};
    c[ImGuiCol_ResizeGripHovered]    = {0.24f, 0.52f, 0.96f, 0.55f};
    c[ImGuiCol_ResizeGripActive]     = {0.24f, 0.52f, 0.96f, 0.90f};

    // Text
    c[ImGuiCol_Text]                 = {0.88f, 0.91f, 0.95f, 1.00f};
    c[ImGuiCol_TextDisabled]         = {0.37f, 0.43f, 0.54f, 1.00f};
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
    const ImU32 colBox = IM_COL32(255, 80,  80,  220);
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
            dl->AddRect(ImVec2(minSX, minSY), ImVec2(maxSX, maxSY), colBox, 0.0f, 0, 1.5f);

        if (g_settings.drawName || g_settings.drawDistance)
        {
            const char* name = g_settings.drawName ? t.name.c_str() : "";
            char distBuf[16] = {};
            const char* dist = "";
            if (g_settings.drawDistance)
            {
                snprintf(distBuf, sizeof(distBuf), "%.1fm", (float)std::sqrt(distSq));
                dist = distBuf;
            }

            ImVec2 nameSize = name[0] ? ImGui::CalcTextSize(name) : ImVec2(0, 0);
            ImVec2 distSize = dist[0] ? ImGui::CalcTextSize(dist) : ImVec2(0, 0);
            const float gap = (name[0] && dist[0]) ? 6.0f : 0.0f;
            const float padX = 6.0f;
            const float padY = 2.0f;
            const float contentW = nameSize.x + gap + distSize.x;
            const float contentH = (std::max)(nameSize.y, distSize.y);

            const float cx = (minSX + maxSX) * 0.5f;
            const float bgTop    = minSY - contentH - padY * 2.0f - 3.0f;
            const float bgLeft   = cx - contentW * 0.5f - padX;
            const float bgRight  = cx + contentW * 0.5f + padX;
            const float bgBottom = bgTop + contentH + padY * 2.0f;
            const float rounding = (bgBottom - bgTop) * 0.5f;

            const ImU32 bgCol     = IM_COL32(12,  14, 20, 200);
            const ImU32 borderCol = IM_COL32(80,  90, 110, 180);
            const ImU32 nameCol   = IM_COL32(255, 255, 255, 240);
            const ImU32 distCol   = IM_COL32(170, 200, 255, 220);

            dl->AddRectFilled(ImVec2(bgLeft, bgTop), ImVec2(bgRight, bgBottom), bgCol, rounding);
            dl->AddRect      (ImVec2(bgLeft, bgTop), ImVec2(bgRight, bgBottom), borderCol, rounding, 0, 1.0f);

            float cursorX = bgLeft + padX;
            const float textY = bgTop + padY + (contentH - nameSize.y) * 0.5f;
            if (name[0]) { dl->AddText(ImVec2(cursorX, textY), nameCol, name); cursorX += nameSize.x + gap; }
            if (dist[0]) { dl->AddText(ImVec2(cursorX, bgTop + padY + (contentH - distSize.y) * 0.5f), distCol, dist); }
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
            ImGui::Begin("AutoClicker", nullptr,
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoCollapse  |
                ImGuiWindowFlags_NoResize);

            // Toggles
            ImGui::Spacing();
            ImGui::Checkbox("Enabled",      &g_settings.acEnabled);
            ImGui::Checkbox("Break Blocks", &g_settings.breakBlocks);

            // CPS
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderInt("##cps", &g_settings.cps, 1, 50);
            ImGui::SameLine(0, 0);
            ImGui::TextDisabled(" CPS: %d", g_settings.cps);

            // ESP section
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Checkbox("ESP",          &g_settings.espEnabled);
            if (g_settings.espEnabled)
            {
                ImGui::Indent();
                ImGui::Checkbox("Glow",     &g_settings.useGlow);
                ImGui::Checkbox("Box",      &g_settings.drawBox);
                ImGui::Checkbox("Name",     &g_settings.drawName);
                ImGui::Checkbox("Distance", &g_settings.drawDistance);
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderInt("##maxdist", &g_settings.maxDistance, 5, 256);
                ImGui::SameLine(0, 0);
                ImGui::TextDisabled(" Max: %dm", g_settings.maxDistance);

                // Diagnostics — copy the snapshot stats under lock and show them
                ImGui::Spacing();
                ImGui::TextDisabled("Diagnostics");
                EspModule::Snapshot snap;
                {
                    std::lock_guard<std::mutex> lk(EspModule::snapMutex);
                    snap = EspModule::snapshot;
                }
                ImGui::Text("valid=%d  mc=%d  lp=%d  lvl=%d  gr=%d  cam=%d",
                    snap.valid, snap.gotMinecraft, snap.gotLocalPlayer,
                    snap.gotLevel, snap.gotGameRenderer, snap.gotCamera);
                ImGui::Text("players()=%d  targets=%d  glow ok=%d fail=%d  jvmtiHook=%d",
                    snap.rawPlayerCount, (int)snap.targets.size(),
                    snap.glowCallsOk, snap.glowCallsFail,
                    JvmtiAgent::IsActive() ? 1 : 0);
                ImGui::Text("cam=(%.1f,%.1f,%.1f)  yaw=%.1f  pitch=%.1f  fov=%.1f",
                    snap.cam.x, snap.cam.y, snap.cam.z,
                    snap.cam.yRot, snap.cam.xRot, snap.cam.fov);

                ImGui::Unindent();
            }

            // Unload
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button,        {0.48f, 0.07f, 0.07f, 1.00f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.70f, 0.10f, 0.10f, 1.00f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.38f, 0.05f, 0.05f, 1.00f});
            if (ImGui::Button("Unload", {-1, 0}))
                g_settings.selfDestruct = true;
            ImGui::PopStyleColor(3);

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
