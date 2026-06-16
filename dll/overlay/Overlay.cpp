#include "Overlay.h"
#include <Windows.h>
#include <gl/GL.h>
#include <climits>
#include <cmath>
#include <memory>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
#include "OverlayWidgets.h"
#include "tabs/Tabs.h"
#include "../Settings.h"
#include "../modules/esp/EspModule.h"
#include "../SDK/Lunar.h"
#include "../SDK/Minecraft.h"
#include "../SDK/Vec3.h"
#include <MinHook.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef BOOL(WINAPI* fn_wglSwapBuffers)(HDC);
static fn_wglSwapBuffers o_wglSwapBuffers = nullptr;

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
static int     s_currentTab         = 0;
static HWND    s_hwnd               = nullptr;
static WNDPROC s_origProc           = nullptr;
static bool    s_eatEscUntilRelease = false;

static bool ProjectWorld(double wx, double wy, double wz,
                         const EspModule::CameraState& cam,
                         float dispW, float dispH, ImVec2& out)
{
    double dx = wx - cam.x;
    double dy = wy - cam.y;
    double dz = wz - cam.z;

    double yawRad   = cam.yRot * M_PI / 180.0;
    double pitchRad = cam.xRot * M_PI / 180.0;

    double cy = std::cos(yawRad);
    double sy = std::sin(yawRad);
    double x1 =  dx * cy + dz * sy;
    double z1 = -dx * sy + dz * cy;
    double y1 =  dy;

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

    out.x = (float)(dispW * 0.5 - (x2 / z2) * f / aspect * dispW * 0.5);
    out.y = (float)(dispH * 0.5 - (y2 / z2) * f                * dispH * 0.5);
    return true;
}

static void EnsureRenderThreadEnv()
{
    if (lc->env != nullptr) return;
    if (lc->vm  == nullptr) return;
    if (!lc->classesLoaded.load(std::memory_order_acquire)) return;
    JNIEnv* env = nullptr;
    if (lc->vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK && env)
        lc->env = env;
}

static void RefreshCameraFromRenderThread(EspModule::CameraState& cam, float& partial)
{
    EnsureRenderThreadEnv();
    if (lc->env == nullptr) return;
    if (lc->env->PushLocalFrame(32) != 0) { lc->env->ExceptionClear(); return; }

    Minecraft mc;
    if (jobject mcInst = mc.GetInstance()) {
        float pt = 1.0f;
        DeltaTracker dt = mc.GetDeltaTracker();
        if (dt.GetInstance() != nullptr) {
            float ptRead = dt.getPartialTick(true);
            if (!lc->env->ExceptionCheck()) {
                pt = ptRead;
                partial = ptRead;
            }
        }

        GameRenderer gr = mc.GetGameRenderer();
        if (gr.GetInstance() != nullptr) {
            Camera cam2 = gr.getMainCamera();
            if (cam2.GetInstance() != nullptr) {
                Vec3 cp = cam2.getPosition();
                if (cp.GetInstance() != nullptr) {
                    cam.x = cp.getX();
                    cam.y = cp.getY();
                    cam.z = cp.getZ();
                }
                cam.yRot = cam2.getYRot();
                cam.xRot = cam2.getXRot();
                float fov = gr.getFov(cam2, pt, true);
                if (!lc->env->ExceptionCheck() && fov > 0.0f) cam.fov = fov;
            }
        }
    }

    if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
    lc->env->PopLocalFrame(nullptr);
}

static void DrawEsp(float dispW, float dispH)
{
    std::shared_ptr<const EspModule::Snapshot> snapPtr = EspModule::Acquire();
    if (!snapPtr || !snapPtr->valid) return;
    const EspModule::Snapshot& snap = *snapPtr;

    EspModule::CameraState cam = snap.cam;
    float partial = snap.partialTick;
    RefreshCameraFromRenderThread(cam, partial);

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float maxDistSq = (float)g_settings.maxDistance * (float)g_settings.maxDistance;

    const double pt = (double)partial;

    for (const auto& t : snap.targets)
    {
        const double ix = t.prevX + (t.x - t.prevX) * pt;
        const double iy = t.prevY + (t.y - t.prevY) * pt;
        const double iz = t.prevZ + (t.z - t.prevZ) * pt;

        double ddx = ix - cam.x;
        double ddy = iy - cam.y;
        double ddz = iz - cam.z;
        double distSq = ddx*ddx + ddy*ddy + ddz*ddz;
        if (distSq > maxDistSq) continue;

        const double minX = ix - t.halfWidth, maxX = ix + t.halfWidth;
        const double minY = iy,               maxY = iy + t.height;
        const double minZ = iz - t.halfDepth, maxZ = iz + t.halfDepth;

        const double cx[2] = {minX, maxX};
        const double cyv[2] = {minY, maxY};
        const double cz[2] = {minZ, maxZ};

        ImVec2 sp[8];
        bool   ok[8];
        float minSX =  1e9f, minSY =  1e9f;
        float maxSX = -1e9f, maxSY = -1e9f;
        bool any = false;

        for (int i = 0; i < 8; ++i)
        {
            double wx = cx[(i >> 0) & 1];
            double wy = cyv[(i >> 1) & 1];
            double wz = cz[(i >> 2) & 1];
            ImVec2 p;
            ok[i] = ProjectWorld(wx, wy, wz, cam, dispW, dispH, p);
            if (!ok[i]) continue;
            sp[i].x = std::floor(p.x + 0.5f);
            sp[i].y = std::floor(p.y + 0.5f);
            any = true;
            if (sp[i].x < minSX) minSX = sp[i].x;
            if (sp[i].y < minSY) minSY = sp[i].y;
            if (sp[i].x > maxSX) maxSX = sp[i].x;
            if (sp[i].y > maxSY) maxSY = sp[i].y;
        }
        if (!any) continue;

        if (maxSX < 0 || minSX > dispW || maxSY < 0 || minSY > dispH) continue;

        const bool tintFriend = t.isFriend && g_settings.highlightFriends;
        const ImU32 friendCol = IM_COL32(85, 255, 85, 255);

        if (g_settings.drawBox)
        {
            static const int edges[12][2] = {
                {0,1},{1,3},{3,2},{2,0},
                {4,5},{5,7},{7,6},{6,4},
                {0,4},{1,5},{2,6},{3,7},
            };
            const uint32_t srcCol = tintFriend ? friendCol : t.boxColor;
            const ImU32 colBox = (srcCol & 0x00FFFFFFu) | (220u << 24);
            for (int e = 0; e < 12; ++e) {
                const int a = edges[e][0], b = edges[e][1];
                if (!ok[a] || !ok[b]) continue;
                dl->AddLine(sp[a], sp[b], colBox, 1.8f);
            }
        }

        if (g_settings.drawName || g_settings.drawDistance || g_settings.drawHealth)
        {
            struct Chunk { std::string text; ImU32 col; };
            std::vector<Chunk> chunks;
            if (g_settings.drawName) {
                for (const auto& nc : t.nameChunks)
                    if (!nc.first.empty()) {
                        const ImU32 col = tintFriend ? friendCol : (ImU32)nc.second;
                        chunks.push_back({nc.first, col});
                    }
            }
            if (g_settings.drawHealth && t.health >= 0.0f && t.maxHealth > 0.0f) {
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

            ImVec2 tagPos;
            if (!ProjectWorld(ix, iy + t.height + 0.5, iz, cam, dispW, dispH, tagPos))
                continue;

            constexpr float NAMETAG_Y_SHIFT_PX = 4.0f;
            constexpr float WORLD_TEXT_HEIGHT  = 0.225f;
            constexpr float FONT_SCALE         = 1.2f;
            double fovRad = cam.fov * M_PI / 180.0;
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

static void ApplyStyle()
{
    using OverlayWidgets::FromHex;
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowPadding      = {12.0f, 12.0f};
    s.FramePadding       = { 7.0f,  4.0f};
    s.ItemSpacing        = { 8.0f,  7.0f};
    s.ItemInnerSpacing   = { 6.0f,  4.0f};
    s.IndentSpacing      = 14.0f;
    s.WindowRounding     = 13.0f;
    s.FrameRounding      = 8.0f;
    s.GrabRounding       = 10.0f;
    s.TabRounding        = 10.0f;
    s.TabBarBorderSize   = 0.0f;
    s.TabBorderSize      = 0.0f;
    s.ScrollbarRounding  = 6.0f;
    s.ScrollbarSize      = 9.0f;
    s.ChildRounding      = 10.0f;
    s.PopupRounding      = 8.0f;
    s.WindowBorderSize   = 1.0f;
    s.ChildBorderSize    = 0.0f;
    s.FrameBorderSize    = 1.0f;
    s.WindowMinSize      = {220.0f, 80.0f};

    ImVec4* c = s.Colors;

    c[ImGuiCol_WindowBg]              = FromHex(0x18181a, 0.80f);
    c[ImGuiCol_ChildBg]               = FromHex(0x202023, 0.42f);
    c[ImGuiCol_PopupBg]               = FromHex(0x1b1b1d, 0.95f);

    c[ImGuiCol_Border]                = FromHex(0x9a9a9f, 0.26f);
    c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);

    c[ImGuiCol_FrameBg]               = FromHex(0x2a2a2e, 0.58f);
    c[ImGuiCol_FrameBgHovered]        = FromHex(0x35353a, 0.68f);
    c[ImGuiCol_FrameBgActive]         = FromHex(0x3f3f45, 0.80f);

    c[ImGuiCol_TitleBg]               = FromHex(0x18181a, 0.0f);
    c[ImGuiCol_TitleBgActive]         = FromHex(0x202023, 0.0f);
    c[ImGuiCol_TitleBgCollapsed]      = FromHex(0x18181a, 0.0f);

    c[ImGuiCol_CheckMark]             = FromHex(0xffffff);
    c[ImGuiCol_SliderGrab]            = FromHex(0xf0f0f2);
    c[ImGuiCol_SliderGrabActive]      = FromHex(0xffffff);

    c[ImGuiCol_Button]                = FromHex(0x36363b, 0.52f);
    c[ImGuiCol_ButtonHovered]         = FromHex(0x434348, 0.68f);
    c[ImGuiCol_ButtonActive]          = FromHex(0x4f4f56, 0.82f);

    c[ImGuiCol_Header]                = FromHex(0x9a9a9f, 0.22f);
    c[ImGuiCol_HeaderHovered]         = FromHex(0x9a9a9f, 0.40f);
    c[ImGuiCol_HeaderActive]          = FromHex(0x9a9a9f, 0.60f);

    c[ImGuiCol_Separator]             = FromHex(0x9a9a9f, 0.20f);
    c[ImGuiCol_SeparatorHovered]      = FromHex(0xb5b5bb, 0.40f);
    c[ImGuiCol_SeparatorActive]       = FromHex(0xcfcfd4, 0.70f);

    c[ImGuiCol_Tab]                   = FromHex(0x18181a, 0.0f);
    c[ImGuiCol_TabHovered]            = FromHex(0x2c2c31, 0.5f);
    c[ImGuiCol_TabActive]             = FromHex(0x2a2a2e, 0.6f);
    c[ImGuiCol_TabUnfocused]          = FromHex(0x18181a, 0.0f);
    c[ImGuiCol_TabUnfocusedActive]    = FromHex(0x2a2a2e, 0.6f);
    c[ImGuiCol_TabSelectedOverline]   = ImVec4(0, 0, 0, 0);

    c[ImGuiCol_ScrollbarBg]           = FromHex(0x18181a, 0.0f);
    c[ImGuiCol_ScrollbarGrab]         = FromHex(0x9a9a9f, 0.28f);
    c[ImGuiCol_ScrollbarGrabHovered]  = FromHex(0xb5b5bb, 0.45f);
    c[ImGuiCol_ScrollbarGrabActive]   = FromHex(0xcfcfd4, 0.65f);

    c[ImGuiCol_ResizeGrip]            = FromHex(0x9a9a9f, 0.15f);
    c[ImGuiCol_ResizeGripHovered]     = FromHex(0xb5b5bb, 0.50f);
    c[ImGuiCol_ResizeGripActive]      = FromHex(0xcfcfd4, 0.80f);

    c[ImGuiCol_Text]                  = FromHex(0xdadade);
    c[ImGuiCol_TextDisabled]          = FromHex(0x808086);
}

static BOOL WINAPI hk_SetCursorPos(int x, int y)
{
    if (s_visible) return TRUE;
    return o_SetCursorPos(x, y);
}

static BOOL WINAPI hk_ClipCursor(const RECT* r)
{
    if (s_visible) return TRUE;
    return o_ClipCursor(r);
}

static int WINAPI hk_ShowCursor(BOOL show)
{
    if (s_visible && !show) return 0;
    return o_ShowCursor(show);
}

static HCURSOR WINAPI hk_SetCursor(HCURSOR cursor)
{
    if (s_visible && cursor == nullptr) {
        static HCURSOR s_arrow = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        return o_SetCursor(s_arrow);
    }
    return o_SetCursor(cursor);
}

typedef UINT(WINAPI* fn_GetRawInputBuffer)(PRAWINPUT, PUINT, UINT);
typedef UINT(WINAPI* fn_GetRawInputData)(HRAWINPUT, UINT, LPVOID, PUINT, UINT);
static fn_GetRawInputBuffer o_GetRawInputBuffer = nullptr;
static fn_GetRawInputData   o_GetRawInputData   = nullptr;

#define ESP_NEXT_RAWINPUT_BLOCK(ptr) \
    ((PRAWINPUT)(((ULONG_PTR)((PBYTE)(ptr) + (ptr)->header.dwSize) + 7) & ~(ULONG_PTR)7))

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

static void ReleaseAllHeldInputs()
{
    if (!s_hwnd || !s_origProc) return;

    for (int vk = 0x01; vk <= 0xFE; ++vk) {
        if (vk == VK_INSERT) continue;
        if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON ||
            vk == VK_XBUTTON1 || vk == VK_XBUTTON2) continue;

        if (GetAsyncKeyState(vk) & 0x8000) {
            const UINT scan = MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_VSC);
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
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);

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

        char fontBold[MAX_PATH]      = {};
        char fontSemibold[MAX_PATH]  = {};
        char fontRegular[MAX_PATH]   = {};
        char fontIcons[MAX_PATH]     = {};
        char fontIconsMDL2[MAX_PATH] = {};
        strcpy_s(fontBold,      sizeof(fontBold),      winDir); strcat_s(fontBold,      sizeof(fontBold),      "\\Fonts\\segoeuib.ttf");
        strcpy_s(fontSemibold,  sizeof(fontSemibold),  winDir); strcat_s(fontSemibold,  sizeof(fontSemibold),  "\\Fonts\\seguisb.ttf");
        strcpy_s(fontRegular,   sizeof(fontRegular),   winDir); strcat_s(fontRegular,   sizeof(fontRegular),   "\\Fonts\\segoeui.ttf");
        strcpy_s(fontIcons,     sizeof(fontIcons),     winDir); strcat_s(fontIcons,     sizeof(fontIcons),     "\\Fonts\\SegoeIcons.ttf");
        strcpy_s(fontIconsMDL2, sizeof(fontIconsMDL2), winDir); strcat_s(fontIconsMDL2, sizeof(fontIconsMDL2), "\\Fonts\\segmdl2.ttf");

        ImFontConfig cfg;
        cfg.OversampleH = 3;
        cfg.OversampleV = 3;
        cfg.PixelSnapH  = false;

        const char* bodyPath =
            (GetFileAttributesA(fontSemibold) != INVALID_FILE_ATTRIBUTES) ? fontSemibold :
            (GetFileAttributesA(fontRegular)  != INVALID_FILE_ATTRIBUTES) ? fontRegular  : nullptr;
        ImFont* fReg = bodyPath
            ? io.Fonts->AddFontFromFileTTF(bodyPath, 16.0f, &cfg) : nullptr;
        if (!fReg) io.Fonts->AddFontDefault();

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
        HWND liveHwnd = WindowFromDC(hdc);
        if (liveHwnd && liveHwnd != s_hwnd) {
            if (s_hwnd && s_origProc)
                SetWindowLongPtrW(s_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(s_origProc));
            s_hwnd     = liveHwnd;
            s_origProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(s_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc)));
            ImGui_ImplWin32_Shutdown();
            ImGui_ImplWin32_Init(s_hwnd);
        }

        if (s_hwnd) {
            WNDPROC current = reinterpret_cast<WNDPROC>(
                GetWindowLongPtrW(s_hwnd, GWLP_WNDPROC));
            if (current && current != HookedWndProc) {
                s_origProc = current;
                SetWindowLongPtrW(s_hwnd, GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(HookedWndProc));
            }
        }
    }

    {
        const bool listeningSuppress = OverlayWidgets::IsKeybindListening();
        OverlayWidgets::ResetKeybindListening();

        const bool menuValid = (g_settings.menuKey > 0 && g_settings.menuKey <= 0xFE);
        const int  menuVk    = menuValid ? g_settings.menuKey : VK_RSHIFT;
        const bool menuEdge  = (GetAsyncKeyState(menuVk) & 1) != 0;

        const bool escEdge  = s_visible && !listeningSuppress &&
                              ((GetAsyncKeyState(VK_ESCAPE) & 1) != 0);

        if (menuEdge || escEdge) {
            const bool wasVisible = s_visible;
            s_visible = menuEdge ? !s_visible : false;
            ClipCursor(nullptr);
            ReleaseCapture();
            static int s_cursorPumpCount = 0;
            if (s_visible) {
                int safety = 256;
                int pumped = 0;
                while (safety-- > 0 && o_ShowCursor(TRUE) < 0) { pumped++; }
                s_cursorPumpCount = pumped;

                POINT op;
                if (GetCursorPos(&op) && s_hwnd) {
                    ScreenToClient(s_hwnd, &op);
                    ImGui::GetIO().AddMousePosEvent((float)op.x, (float)op.y);
                }

                if (o_SetCursor) {
                    o_SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)));
                }
            } else if (wasVisible) {
                while (s_cursorPumpCount > 0) {
                    o_ShowCursor(FALSE);
                    s_cursorPumpCount--;
                }
            }
            if (s_visible && !wasVisible) ReleaseAllHeldInputs();
            if (!s_visible && wasVisible) g_settings.Save();
            if (escEdge) s_eatEscUntilRelease = true;
        }

        if (s_eatEscUntilRelease && !(GetAsyncKeyState(VK_ESCAPE) & 0x8000))
            s_eatEscUntilRelease = false;

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

        if (s_visible) {
            ImGuiIO& io = ImGui::GetIO();
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
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                ImVec2(0, 0), display, IM_COL32(9, 9, 11, 150));

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::SetNextWindowPos (ImVec2(display.x * 0.5f, display.y * 0.5f),
                                     ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(638, 380), ImGuiCond_Always);
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

            {
                ImDrawList* wdl = ImGui::GetWindowDrawList();
                const float R = ImGui::GetStyle().WindowRounding;
                wdl->AddRectFilledMultiColor(
                    ImVec2(winPos.x + R, winPos.y + 1.0f),
                    ImVec2(winPos.x + winSize.x - R, winPos.y + 48.0f),
                    IM_COL32(255, 255, 255, 24), IM_COL32(255, 255, 255, 24),
                    IM_COL32(255, 255, 255, 0),  IM_COL32(255, 255, 255, 0));
                wdl->AddLine(
                    ImVec2(winPos.x + R, winPos.y + 1.0f),
                    ImVec2(winPos.x + winSize.x - R, winPos.y + 1.0f),
                    IM_COL32(255, 255, 255, 64), 1.0f);
            }

            ImGui::BeginChild("##sidebar", ImVec2(SIDEBAR_W, winSize.y),
                false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
            {
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

                ImGui::SetCursorPos(ImVec2(0, 76));
                using OverlayWidgets::SidebarTab;
                if (SidebarTab("Autoclicker", s_currentTab == 0)) s_currentTab = 0;
                if (SidebarTab("Aim",         s_currentTab == 1)) s_currentTab = 1;
                if (SidebarTab("ESP",         s_currentTab == 2)) s_currentTab = 2;
                if (SidebarTab("Friends",     s_currentTab == 3)) s_currentTab = 3;
                if (SidebarTab("Macros",      s_currentTab == 4)) s_currentTab = 4;
                if (SidebarTab("Misc",        s_currentTab == 5)) s_currentTab = 5;
                if (SidebarTab("Settings",    s_currentTab == 6)) s_currentTab = 6;
            }
            ImGui::EndChild();

            ImGui::SameLine(0, 0);

            ImGui::BeginChild("##content", ImVec2(winSize.x - SIDEBAR_W, winSize.y),
                false, ImGuiWindowFlags_NoBackground);
            {
                ImGui::SetCursorPos(ImVec2(22, 22));
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
                ImGui::TextUnformatted(
                    s_currentTab == 0 ? "Autoclicker" :
                    s_currentTab == 1 ? "Aimassist"  :
                    s_currentTab == 2 ? "ESP"         :
                    s_currentTab == 3 ? "Friends"     :
                    s_currentTab == 4 ? "Macros"      :
                    s_currentTab == 5 ? "Misc"        :
                                        "Settings");
                ImGui::PopFont();

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

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

                bool dirty = false;
                switch (s_currentTab)
                {
                case 0: dirty = OverlayTabs::RenderAutoclicker(); break;
                case 1: dirty = OverlayTabs::RenderAimassist();   break;
                case 2: dirty = OverlayTabs::RenderEsp();         break;
                case 3: dirty = OverlayTabs::RenderFriends();     break;
                case 4: dirty = OverlayTabs::RenderMacros();      break;
                case 5: dirty = OverlayTabs::RenderMisc();        break;
                case 6: dirty = OverlayTabs::RenderSettings();    break;
                }

                if (dirty) g_settings.Save();

                ImGui::PopStyleVar();

                ImGui::EndChild();
            }
            ImGui::EndChild();

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
