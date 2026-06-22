#include "Overlay.h"
#include <Windows.h>
#include <gl/GL.h>
#include <atomic>
#include <climits>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
#include "OverlayWidgets.h"
#include "Notifications.h"
#include "tabs/Tabs.h"
#include "../config/Settings.h"
#include "../logger/Logger.h"
#include "../modules/esp/EspModule.h"
#include "../modules/scaffold/ScaffoldModule.h"
#include "../SDK/Lunar.h"
#include "Mappings.h"
#include "../SDK/Capabilities.h"
#include "../SDK/View.h"
#include "../SDK/Minecraft.h"
#include "../SDK/Screen.h"
#include "../SDK/Vec3.h"
#include "Platform.h"
#include "Revision.h"
#include "LogoData.h"
#include <MinHook.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef BOOL(WINAPI* fn_wglSwapBuffers)(HDC);
static fn_wglSwapBuffers o_wglSwapBuffers = nullptr;

typedef HGLRC(WINAPI* fn_wglGetCurrentContext)();
static fn_wglGetCurrentContext p_wglGetCurrentContext = nullptr;

static void* DbgGlCtx()
{
    if (!p_wglGetCurrentContext) {
        HMODULE gl = GetModuleHandleA("opengl32.dll");
        if (gl) p_wglGetCurrentContext =
            reinterpret_cast<fn_wglGetCurrentContext>(GetProcAddress(gl, "wglGetCurrentContext"));
    }
    return p_wglGetCurrentContext ? reinterpret_cast<void*>(p_wglGetCurrentContext()) : nullptr;
}

static void* GlProc(const char* name)
{
    HMODULE gl = GetModuleHandleA("opengl32.dll");
    if (!gl) return nullptr;
    typedef PROC(WINAPI* fn_wglGetProcAddress)(LPCSTR);
    static fn_wglGetProcAddress p_get =
        reinterpret_cast<fn_wglGetProcAddress>(GetProcAddress(gl, "wglGetProcAddress"));
    void* p = p_get ? reinterpret_cast<void*>(p_get(name)) : nullptr;
    if (!p) p = reinterpret_cast<void*>(GetProcAddress(gl, name));
    return p;
}

static void ResetUnpackState()
{
    typedef void (WINAPI* fn_glBindBuffer)(GLenum, GLuint);
    static fn_glBindBuffer p_glBindBuffer =
        reinterpret_cast<fn_glBindBuffer>(GlProc("glBindBuffer"));
    if (p_glBindBuffer) p_glBindBuffer(0x88EC, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,  0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS,   0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT,   1);
}

static GLuint s_logoTex = 0;
static int    s_logoW   = 0;
static int    s_logoH   = 0;

static void LoadLogoTexture()
{
    if (s_logoTex != 0 || g_logoPngSize == 0) return;

    int ch = 0;
    unsigned char* pixels = stbi_load_from_memory(
        g_logoPng, (int)g_logoPngSize, &s_logoW, &s_logoH, &ch, 4);
    if (!pixels) {
        LOG("overlay: logo decode failed: %s", stbi_failure_reason());
        return;
    }

    ResetUnpackState();
    glGenTextures(1, &s_logoTex);
    glBindTexture(GL_TEXTURE_2D, s_logoTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s_logoW, s_logoH, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    stbi_image_free(pixels);
}

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

static std::atomic<bool> s_gameScreenOpen{false};
static std::atomic<bool> s_nonChatScreenOpen{false};

static volatile bool s_shutdownRequested = false;
static volatile bool s_renderDrained     = false;

static bool ProjectWorld(double wx, double wy, double wz,
                         const EspModule::CameraState& cam,
                         float dispW, float dispH, ImVec2& out)
{
    if (cam.hasMatrix)
    {
        const float* mv = cam.modelview;
        const float* pr = cam.projection;
        const double rx = wx - cam.x;
        const double ry = wy - cam.y;
        const double rz = wz - cam.z;

        const double ex = mv[0] * rx + mv[4] * ry + mv[8]  * rz + mv[12];
        const double ey = mv[1] * rx + mv[5] * ry + mv[9]  * rz + mv[13];
        const double ez = mv[2] * rx + mv[6] * ry + mv[10] * rz + mv[14];

        const double cx = pr[0] * ex + pr[4] * ey + pr[8]  * ez + pr[12];
        const double cy = pr[1] * ex + pr[5] * ey + pr[9]  * ez + pr[13];
        const double cw = pr[3] * ex + pr[7] * ey + pr[11] * ez + pr[15];

        if (cw <= 1e-4) return false;

        const double ndcX = cx / cw;
        const double ndcY = cy / cw;
        const double vpW = (cam.viewport[2] > 0) ? (double)cam.viewport[2] : (double)dispW;
        const double vpH = (cam.viewport[3] > 0) ? (double)cam.viewport[3] : (double)dispH;
        out.x = (float)((ndcX * 0.5 + 0.5) * vpW);
        out.y = (float)((1.0 - (ndcY * 0.5 + 0.5)) * vpH);
        return true;
    }

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

static bool IsGameScreenOpen(bool& chatOpen)
{
    chatOpen = false;
    EnsureRenderThreadEnv();
    if (lc->env == nullptr) return false;
    if (lc->env->PushLocalFrame(8) != 0) { lc->env->ExceptionClear(); return false; }

    bool open = false;
    Minecraft mc;
    if (mc.GetInstance() != nullptr) {
        Screen screen = mc.GetScreen();
        if (screen.GetInstance() != nullptr) {
            open = true;
            static jclass chatCls = nullptr;
            JClass(chatCls, MC_ChatScreen);
            if (chatCls && lc->env->IsInstanceOf(screen.GetInstance(), chatCls) == JNI_TRUE)
                chatOpen = true;
        }
    }

    if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
    lc->env->PopLocalFrame(nullptr);
    return open;
}

static void RefreshCameraFromRenderThread(EspModule::CameraState& cam, float& partial)
{
    EnsureRenderThreadEnv();
    if (lc->env == nullptr) return;
    if (lc->env->PushLocalFrame(32) != 0) { lc->env->ExceptionClear(); return; }

    Minecraft mc;
    if (mc.GetInstance() != nullptr) {
        Player localPlayer = mc.GetLocalPlayer();
        if (localPlayer.GetInstance() != nullptr) {
            ViewState v = AcquireView(mc, localPlayer);
            if (v.ok) {
                cam.x = v.x;
                cam.y = v.y;
                cam.z = v.z;
                cam.yRot = v.yRot;
                cam.xRot = v.xRot;
                if (v.fov > 0.0f) cam.fov = v.fov;
                partial = v.partialTick;
                cam.hasMatrix = v.hasMatrix;
                if (v.hasMatrix) {
                    for (int i = 0; i < 16; ++i) { cam.modelview[i] = v.modelview[i]; cam.projection[i] = v.projection[i]; }
                    for (int i = 0; i < 4; ++i) cam.viewport[i] = v.viewport[i];
                }
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
    s.FramePadding       = {11.0f,  4.0f};
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

    s.ScaleAllSizes(Theme::Scale);

    ImVec4* c = s.Colors;

    c[ImGuiCol_WindowBg]              = FromHex(Theme::WindowBg);
    c[ImGuiCol_ChildBg]               = FromHex(Theme::ChildBg);
    c[ImGuiCol_PopupBg]               = FromHex(Theme::PopupBg);

    c[ImGuiCol_Border]                = FromHex(Theme::Border);
    c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);

    c[ImGuiCol_FrameBg]               = FromHex(Theme::FrameBg);
    c[ImGuiCol_FrameBgHovered]        = FromHex(Theme::FrameBgHovered);
    c[ImGuiCol_FrameBgActive]         = FromHex(Theme::FrameBgActive);

    c[ImGuiCol_TitleBg]               = FromHex(Theme::Transparent);
    c[ImGuiCol_TitleBgActive]         = FromHex(Theme::Transparent);
    c[ImGuiCol_TitleBgCollapsed]      = FromHex(Theme::Transparent);

    c[ImGuiCol_CheckMark]             = FromHex(Theme::White);
    c[ImGuiCol_SliderGrab]            = FromHex(Theme::Knob);
    c[ImGuiCol_SliderGrabActive]      = FromHex(Theme::KnobActive);

    c[ImGuiCol_Button]                = FromHex(Theme::Button);
    c[ImGuiCol_ButtonHovered]         = FromHex(Theme::ButtonHovered);
    c[ImGuiCol_ButtonActive]          = FromHex(Theme::ButtonActive);

    c[ImGuiCol_Header]                = FromHex(Theme::Header);
    c[ImGuiCol_HeaderHovered]         = FromHex(Theme::HeaderHovered);
    c[ImGuiCol_HeaderActive]          = FromHex(Theme::HeaderActive);

    c[ImGuiCol_Separator]             = FromHex(Theme::Separator);
    c[ImGuiCol_SeparatorHovered]      = FromHex(Theme::SeparatorHovered);
    c[ImGuiCol_SeparatorActive]       = FromHex(Theme::SeparatorActive);

    c[ImGuiCol_Tab]                   = FromHex(Theme::Transparent);
    c[ImGuiCol_TabHovered]            = FromHex(Theme::TabBarHovered);
    c[ImGuiCol_TabActive]             = FromHex(Theme::TabBarActive);
    c[ImGuiCol_TabUnfocused]          = FromHex(Theme::Transparent);
    c[ImGuiCol_TabUnfocusedActive]    = FromHex(Theme::TabBarActive);
    c[ImGuiCol_TabSelectedOverline]   = ImVec4(0, 0, 0, 0);

    c[ImGuiCol_ScrollbarBg]           = FromHex(Theme::Transparent);
    c[ImGuiCol_ScrollbarGrab]         = FromHex(Theme::ScrollGrab);
    c[ImGuiCol_ScrollbarGrabHovered]  = FromHex(Theme::ScrollGrabHovered);
    c[ImGuiCol_ScrollbarGrabActive]   = FromHex(Theme::ScrollGrabActive);

    c[ImGuiCol_ResizeGrip]            = FromHex(Theme::ResizeGrip);
    c[ImGuiCol_ResizeGripHovered]     = FromHex(Theme::ResizeGripHovered);
    c[ImGuiCol_ResizeGripActive]      = FromHex(Theme::ResizeGripActive);

    c[ImGuiCol_Text]                  = FromHex(Theme::Text);
    c[ImGuiCol_TextDisabled]          = FromHex(Theme::TextDim);
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
    return o_ShowCursor(show);
}

static HCURSOR WINAPI hk_SetCursor(HCURSOR cursor)
{
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

    if (kNativeDropOverride && !s_visible)
    {
        const int dropVk = g_settings.dropKey;
        if (dropVk > 0 && dropVk <= 0xFE && (int)wParam == dropVk
            && !s_gameScreenOpen.load(std::memory_order_relaxed))
        {
            switch (msg)
            {
            case WM_KEYDOWN:    case WM_KEYUP:
            case WM_SYSKEYDOWN: case WM_SYSKEYUP:
                return 0;
            }
        }
    }

    if (s_visible)
    {
        switch (msg)
        {
        case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
        case WM_MOUSEMOVE:
            break;
        default:
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
            break;
        }

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
    if (s_shutdownRequested)
    {
        if (!s_renderDrained)
        {
            LOG("overlay: render thread draining");
            ScaffoldModule::Release();
            if (s_initialized)
            {
                if (s_hwnd && s_origProc)
                    SetWindowLongPtrW(s_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(s_origProc));
                if (s_logoTex) { glDeleteTextures(1, &s_logoTex); s_logoTex = 0; }
                ImGui_ImplOpenGL3_Shutdown();
                ImGui_ImplWin32_Shutdown();
                ImGui::DestroyContext();
            }
            s_renderDrained = true;
            LOG("overlay: render thread drained");
        }
        return o_wglSwapBuffers(hdc);
    }

    const HWND swapWnd = WindowFromDC(hdc);
    {
        static HWND s_gameWnd = nullptr;
        if (s_gameWnd == nullptr || !IsWindow(s_gameWnd)) {
            HWND g = FindGameWindow();
            s_gameWnd = g ? g : swapWnd;
            LOG("overlay: pinned render window=%p (first swap caller window=%p)",
                   (void*)s_gameWnd, (void*)swapWnd);
        }
        if (s_gameWnd && swapWnd != s_gameWnd)
            return o_wglSwapBuffers(hdc);
    }

    if (!DbgGlCtx())
    {
        static bool s_warnedNoCtx = false;
        if (!s_warnedNoCtx) {
            LOG("overlay: no current GL context on swap tid=%lu hwnd=%p",
                   GetCurrentThreadId(), (void*)swapWnd);
            s_warnedNoCtx = true;
        }
        return o_wglSwapBuffers(hdc);
    }

    if (!s_initialized)
    {
        s_hwnd = swapWnd;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;

        char winDir[MAX_PATH] = {};
        GetWindowsDirectoryA(winDir, MAX_PATH);

        char fontBold[MAX_PATH]      = {};
        char fontSemibold[MAX_PATH]  = {};
        char fontRegular[MAX_PATH]   = {};
        strcpy_s(fontBold,      sizeof(fontBold),      winDir); strcat_s(fontBold,      sizeof(fontBold),      "\\Fonts\\segoeuib.ttf");
        strcpy_s(fontSemibold,  sizeof(fontSemibold),  winDir); strcat_s(fontSemibold,  sizeof(fontSemibold),  "\\Fonts\\seguisb.ttf");
        strcpy_s(fontRegular,   sizeof(fontRegular),   winDir); strcat_s(fontRegular,   sizeof(fontRegular),   "\\Fonts\\segoeui.ttf");

        ImFontConfig cfg;
        cfg.OversampleH = 3;
        cfg.OversampleV = 3;
        cfg.PixelSnapH  = false;

        const char* bodyPath =
            (GetFileAttributesA(fontSemibold) != INVALID_FILE_ATTRIBUTES) ? fontSemibold :
            (GetFileAttributesA(fontRegular)  != INVALID_FILE_ATTRIBUTES) ? fontRegular  : nullptr;
        ImFont* fReg = bodyPath
            ? io.Fonts->AddFontFromFileTTF(bodyPath, Theme::M::FontBody, &cfg) : nullptr;
        if (!fReg) io.Fonts->AddFontDefault();

        ImFont* fBold = (GetFileAttributesA(fontBold) != INVALID_FILE_ATTRIBUTES)
            ? io.Fonts->AddFontFromFileTTF(fontBold, Theme::M::FontTitle, &cfg) : nullptr;
        if (!fBold) io.Fonts->AddFontDefault();

        ApplyStyle();

        ImGui_ImplWin32_Init(s_hwnd);
        ImGui_ImplOpenGL3_Init("#version 150");

        ResetUnpackState();
        ImGui_ImplOpenGL3_CreateFontsTexture();
        ImGui_ImplOpenGL3_CreateDeviceObjects();
        LoadLogoTexture();

        s_origProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(s_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc)));

        s_initialized = true;
        Notifications::Push("Successfully injected", Notifications::Kind::Info);
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

        const bool gameForeground  = (s_hwnd != nullptr && GetForegroundWindow() == s_hwnd);
        bool       chatScreenOpen  = false;
        const bool gameScreenOpen  = IsGameScreenOpen(chatScreenOpen);
        s_gameScreenOpen.store(gameScreenOpen, std::memory_order_relaxed);
        s_nonChatScreenOpen.store(gameScreenOpen && !chatScreenOpen, std::memory_order_relaxed);
        const bool keybindsBlocked = !gameForeground || gameScreenOpen;

        const bool menuValid = (g_settings.menuKey > 0 && g_settings.menuKey <= 0xFE);
        const int  menuVk    = menuValid ? g_settings.menuKey : VK_RSHIFT;
        const bool menuEdge  = (GetAsyncKeyState(menuVk) & 1) != 0;

        const bool escEdge  = s_visible && !listeningSuppress &&
                              ((GetAsyncKeyState(VK_ESCAPE) & 1) != 0);

        const bool openBlocked = menuEdge && !s_visible && keybindsBlocked;

        if ((menuEdge && !openBlocked) || escEdge) {
            const bool wasVisible = s_visible;
            s_visible = menuEdge ? !s_visible : false;
            ClipCursor(nullptr);
            ReleaseCapture();
            if (s_visible) {
                POINT op;
                if (GetCursorPos(&op) && s_hwnd) {
                    ScreenToClient(s_hwnd, &op);
                    ImGui::GetIO().AddMousePosEvent((float)op.x, (float)op.y);
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
        static bool s_scaffoldKeyHeldPrev = false;
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
        const bool scaffoldHeld =
            (g_settings.scaffoldKey > 0 && g_settings.scaffoldKey <= 0xFE) &&
            (GetAsyncKeyState(g_settings.scaffoldKey) & 0x8000);
        const bool destructHeld =
            (g_settings.selfDestructKey > 0 && g_settings.selfDestructKey <= 0xFE) &&
            (GetAsyncKeyState(g_settings.selfDestructKey) & 0x8000);

        auto notifyToggle = [](const char* name, bool on) {
            Notifications::Push(std::string(name) + (on ? " enabled" : " disabled"),
                on ? Notifications::Kind::Enabled : Notifications::Kind::Disabled);
        };

        if (!listeningSuppress && !keybindsBlocked) {
            if (espHeld      && !s_espKeyHeldPrev)      { g_settings.espEnabled      = !g_settings.espEnabled;      notifyToggle("ESP", g_settings.espEnabled); }
            if (acHeld       && !s_acKeyHeldPrev)       { g_settings.acEnabled       = !g_settings.acEnabled;       notifyToggle("Autoclicker", g_settings.acEnabled); }
            if (aimHeld      && !s_aimKeyHeldPrev)      { g_settings.aimEnabled      = !g_settings.aimEnabled;      notifyToggle("Aim assist", g_settings.aimEnabled); }
            if (scaffoldHeld && !s_scaffoldKeyHeldPrev) { g_settings.scaffoldEnabled = !g_settings.scaffoldEnabled; notifyToggle("Scaffold", g_settings.scaffoldEnabled); }
            if (destructHeld && !s_destructKeyHeldPrev) { g_settings.selfDestruct = true; Notifications::Push("Unloading...", Notifications::Kind::Alert); }
        }
        s_espKeyHeldPrev      = espHeld;
        s_acKeyHeldPrev       = acHeld;
        s_aimKeyHeldPrev      = aimHeld;
        s_scaffoldKeyHeldPrev = scaffoldHeld;
        s_destructKeyHeldPrev = destructHeld;
    }

    ScaffoldModule::Tick();

    const bool needFrame = s_visible || g_settings.espEnabled || Notifications::HasActive();
    if (needFrame)
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();

        if (s_visible) {
            ImGuiIO& io = ImGui::GetIO();
            io.MouseDrawCursor = true;

            POINT cp;
            if (GetCursorPos(&cp)) {
                if (s_hwnd) ScreenToClient(s_hwnd, &cp);
                io.AddMousePosEvent((float)cp.x, (float)cp.y);
            }

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

        const bool screenOrMenuOpen =
            s_visible || s_nonChatScreenOpen.load(std::memory_order_relaxed);

        if (g_settings.espEnabled && !screenOrMenuOpen)
            DrawEsp(display.x, display.y);

        if (s_visible)
        {
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                ImVec2(0, 0), display, IM_COL32(9, 9, 11, 150));

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::SetNextWindowPos (ImVec2(display.x * 0.5f, display.y * 0.5f),
                                     ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(Theme::M::WindowW, Theme::M::WindowH), ImGuiCond_Always);
            ImGui::Begin("manuclicker", nullptr,
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize   |
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoMove);
            ImGui::PopStyleVar();

            const float   MARGIN    = Theme::M::Margin;
            const float   TOPBAR_H  = Theme::M::TopbarH;
            const float   BODY_Y    = MARGIN + TOPBAR_H + MARGIN;
            const float   SIDEBAR_W = Theme::M::SidebarW;
            const ImVec2  winSize   = ImGui::GetWindowSize();
            const ImVec2  winPos    = ImGui::GetWindowPos();
            ImDrawList*   dl        = ImGui::GetWindowDrawList();

            dl->AddRectFilled(
                ImVec2(winPos.x + MARGIN,             winPos.y + MARGIN),
                ImVec2(winPos.x + winSize.x - MARGIN, winPos.y + MARGIN + TOPBAR_H),
                IM_COL32(21, 21, 24, 255),
                ImGui::GetStyle().WindowRounding,
                ImDrawFlags_RoundCornersAll);

            float titleX = MARGIN + Theme::M::TitlePadX;
            if (s_logoTex) {
                const float logoH = Theme::M::LogoH;
                const float logoW = logoH * ((float)s_logoW / (float)s_logoH);
                ImGui::SetCursorPos(ImVec2(titleX, MARGIN + (TOPBAR_H - logoH) * 0.5f));
                ImGui::Image((ImTextureID)(intptr_t)s_logoTex, ImVec2(logoW, logoH));
                titleX += logoW + Theme::M::LogoGap;
            }

            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
            const float titleY = MARGIN + (TOPBAR_H - ImGui::GetFontSize()) * 0.5f;
            ImGui::SetCursorPos(ImVec2(titleX, titleY));
            ImGui::TextUnformatted("manuclicker");
            ImGui::PopFont();

            const std::string revStr = "v" + std::string(BUILD_REVISION);
            const ImVec2 revSz = ImGui::CalcTextSize(revStr.c_str());
            ImGui::SetCursorPos(ImVec2(winSize.x - MARGIN - revSz.x - Theme::M::TitlePadX,
                                       MARGIN + (TOPBAR_H - revSz.y) * 0.5f));
            ImGui::TextLinkOpenURL(revStr.c_str(), "https://github.com/expires/autoclicker");

            ImGui::SetCursorPos(ImVec2(0, BODY_Y));
            ImGui::BeginChild("##sidebar", ImVec2(SIDEBAR_W, winSize.y - BODY_Y),
                false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
            {
                dl->AddRectFilled(ImVec2(winPos.x, winPos.y + BODY_Y),
                    ImVec2(winPos.x + SIDEBAR_W, winPos.y + winSize.y),
                    ImGui::GetColorU32(ImGuiCol_ChildBg),
                    ImGui::GetStyle().WindowRounding,
                    ImDrawFlags_RoundCornersBottomLeft);
                dl->AddRectFilled(
                    ImVec2(winPos.x + SIDEBAR_W - 1, winPos.y + BODY_Y),
                    ImVec2(winPos.x + SIDEBAR_W,     winPos.y + winSize.y),
                    ImGui::GetColorU32(ImGuiCol_Border));

                ImGui::SetCursorPos(ImVec2(0, Theme::M::SidebarTopPad));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                using OverlayWidgets::SidebarTab;
                if (SidebarTab("Autoclicker", s_currentTab == 0)) s_currentTab = 0;
                if (SidebarTab("Aim",         s_currentTab == 1)) s_currentTab = 1;
                if (SidebarTab("ESP",         s_currentTab == 2)) s_currentTab = 2;
                if (SidebarTab("Friends",     s_currentTab == 3)) s_currentTab = 3;
                if (SidebarTab("Macros",      s_currentTab == 4)) s_currentTab = 4;
                if (SidebarTab("Misc",        s_currentTab == 5)) s_currentTab = 5;
                if (SidebarTab("Settings",    s_currentTab == 6)) s_currentTab = 6;
                ImGui::PopStyleVar();
            }
            ImGui::EndChild();

            ImGui::SameLine(0, 0);

            ImGui::SetCursorPos(ImVec2(SIDEBAR_W, BODY_Y));
            ImGui::BeginChild("##content", ImVec2(winSize.x - SIDEBAR_W, winSize.y - BODY_Y),
                false, ImGuiWindowFlags_NoBackground);
            {
                const float bodyPadding = Theme::M::BodyPad;
                ImGui::SetCursorPos(ImVec2(bodyPadding, bodyPadding));
                ImGui::BeginChild("##body",
                    ImVec2(winSize.x - SIDEBAR_W - bodyPadding * 2,
                           winSize.y - BODY_Y - bodyPadding * 2),
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

        Notifications::Render(display.x, display.y);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    return o_wglSwapBuffers(hdc);
}

namespace Overlay
{
    bool IsMenuVisible() { return s_visible; }
    bool IsScreenOpen() { return s_gameScreenOpen.load(std::memory_order_relaxed); }

    void Init()
    {
        MH_Initialize();

        void* tgtSwap = reinterpret_cast<void*>(
            GetProcAddress(GetModuleHandleA("opengl32.dll"), "wglSwapBuffers"));
        if (tgtSwap)
            MH_CreateHook(tgtSwap, reinterpret_cast<void*>(hk_wglSwapBuffers),
                          reinterpret_cast<void**>(&o_wglSwapBuffers));

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
                if (target) MH_CreateHook(target, h.hook, h.orig);
            }
        }

        MH_QueueEnableHook(MH_ALL_HOOKS);
        MH_ApplyQueued();
    }

    void BeginTeardown()
    {
        s_shutdownRequested = true;
    }

    void Shutdown()
    {
        s_shutdownRequested = true;

        if (s_initialized && !s_renderDrained)
        {
            for (int i = 0; i < 100 && !s_renderDrained; ++i)
                Sleep(2);

            if (!s_renderDrained && s_hwnd && s_origProc)
                SetWindowLongPtrW(s_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(s_origProc));
        }

        LOG("overlay: disabling hooks");
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        LOG("overlay: hooks disabled");
    }
}
