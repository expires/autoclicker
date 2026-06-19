#include "Notifications.h"
#include "../config/Settings.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <mutex>
#include <vector>

namespace
{
    struct Toast
    {
        std::string                           text;
        Notifications::Kind                   kind;
        std::chrono::steady_clock::time_point born;
    };

    std::vector<Toast> g_toasts;
    std::mutex         g_mutex;

    constexpr double FADE_IN  = 0.20;
    constexpr double HOLD     = 1.40;
    constexpr double FADE_OUT = 0.40;
    constexpr double LIFETIME = HOLD + FADE_OUT;
    constexpr size_t MAX_TOASTS = 6;

    void accentFor(Notifications::Kind k, int& r, int& g, int& b)
    {
        switch (k)
        {
        case Notifications::Kind::Enabled:  r =  90; g = 210; b = 110; break;
        case Notifications::Kind::Disabled: r = 230; g =  90; b =  90; break;
        case Notifications::Kind::Alert:    r = 240; g = 170; b =  70; break;
        default:                            r =  90; g = 170; b = 255; break;
        }
    }
}

namespace Notifications
{
    void Push(const std::string& text, Kind kind)
    {
        if (!g_settings.notificationsEnabled) return;
        std::lock_guard<std::mutex> lk(g_mutex);
        g_toasts.push_back({ text, kind, std::chrono::steady_clock::now() });
        if (g_toasts.size() > MAX_TOASTS)
            g_toasts.erase(g_toasts.begin());
    }

    bool HasActive()
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        return !g_toasts.empty();
    }

    void Render(float dispW, float dispH)
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_toasts.empty()) return;

        const auto now = std::chrono::steady_clock::now();
        g_toasts.erase(std::remove_if(g_toasts.begin(), g_toasts.end(),
            [&](const Toast& t) {
                return std::chrono::duration<double>(now - t.born).count() >= LIFETIME;
            }), g_toasts.end());
        if (g_toasts.empty()) return;

        ImDrawList* dl   = ImGui::GetForegroundDrawList();
        ImGuiIO&    io   = ImGui::GetIO();
        ImFont*     font = (io.Fonts && io.Fonts->Fonts.Size > 0) ? io.Fonts->Fonts[0]
                                                                  : ImGui::GetFont();

        constexpr float FONT_SIZE   = 17.0f;
        constexpr float PAD_X       = 13.0f;
        constexpr float PAD_Y       = 8.0f;
        constexpr float ACCENT_W    = 3.0f;
        constexpr float MARGIN_X    = 16.0f;
        constexpr float MARGIN_BOT  = 16.0f;
        constexpr float GAP         = 8.0f;
        constexpr float ROUNDING    = 6.0f;

        const float rightX = dispW - MARGIN_X;
        float y = dispH - MARGIN_BOT;

        for (auto it = g_toasts.rbegin(); it != g_toasts.rend(); ++it)
        {
            const double e = std::chrono::duration<double>(now - it->born).count();
            float a;
            if      (e < FADE_IN) a = (float)(e / FADE_IN);
            else if (e < HOLD)    a = 1.0f;
            else                  a = 1.0f - (float)((e - HOLD) / FADE_OUT);
            if (a < 0.0f) a = 0.0f;
            if (a > 1.0f) a = 1.0f;

            const ImVec2 ts   = font->CalcTextSizeA(FONT_SIZE, FLT_MAX, 0.0f, it->text.c_str());
            const float  boxW = ts.x + PAD_X * 2.0f + ACCENT_W;
            const float  boxH = ts.y + PAD_Y * 2.0f;

            const float slide = (e < FADE_IN) ? (1.0f - a) * 22.0f : 0.0f;
            const float x1    = rightX + slide;
            const float x0    = x1 - boxW;
            const float y1    = y;
            const float y0    = y1 - boxH;

            int ar, ag, ab;
            accentFor(it->kind, ar, ag, ab);

            const ImU32 bg     = IM_COL32(18, 19, 24, (int)(236 * a));
            const ImU32 border = IM_COL32(80, 90, 110, (int)(170 * a));
            const ImU32 accent = IM_COL32(ar, ag, ab, (int)(255 * a));
            const ImU32 txt    = IM_COL32(236, 239, 245, (int)(255 * a));

            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), bg, ROUNDING);
            dl->AddRect      (ImVec2(x0, y0), ImVec2(x1, y1), border, ROUNDING, 0, 1.0f);
            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + ACCENT_W, y1), accent,
                              ROUNDING, ImDrawFlags_RoundCornersLeft);
            dl->AddText(font, FONT_SIZE, ImVec2(x0 + ACCENT_W + PAD_X, y0 + PAD_Y),
                        txt, it->text.c_str());

            y = y0 - GAP;
        }
    }
}
