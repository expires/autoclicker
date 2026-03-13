#include "Overlay.h"
#include <Windows.h>
#include <gl/GL.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
#include "../Settings.h"
#include <MinHook.h>

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

    if (s_visible)
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        const ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize({display.x * 0.30f, display.y * 0.20f}, ImGuiCond_Always);
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
