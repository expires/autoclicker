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
    constexpr size_t MAX_TOASTS = 2;
}

namespace Notifications
{
    void Push(const std::string& text, Kind kind)
    {
        if (!g_settings.notificationsEnabled) return;

        std::lock_guard<std::mutex> lk(g_mutex);

        g_toasts.push_back({
            text,
            kind,
            std::chrono::steady_clock::now()
        });

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

        g_toasts.erase(
            std::remove_if(
                g_toasts.begin(),
                g_toasts.end(),
                [&](const Toast& t)
                {
                    return std::chrono::duration<double>(now - t.born).count() >= LIFETIME;
                }),
            g_toasts.end());

        if (g_toasts.empty()) return;

        ImDrawList* dl = ImGui::GetForegroundDrawList();

        ImGuiIO& io = ImGui::GetIO();
        ImFont* font =
            (io.Fonts && io.Fonts->Fonts.Size > 0)
            ? io.Fonts->Fonts[0]
            : ImGui::GetFont();

        constexpr float FONT_SIZE  = 20.4f;
        constexpr float PAD_X      = 17.6f;
        constexpr float PAD_Y      = 11.2f;
        constexpr float GAP        = 9.6f;
        constexpr float ROUNDING   = 6.4f;
        constexpr float TOP_MARGIN = 40.0f;

        float y = TOP_MARGIN;

        for (auto it = g_toasts.rbegin(); it != g_toasts.rend(); ++it)
        {
            const double elapsed =
                std::chrono::duration<double>(now - it->born).count();

            float alpha;

            if (elapsed < FADE_IN)
                alpha = static_cast<float>(elapsed / FADE_IN);
            else if (elapsed < HOLD)
                alpha = 1.0f;
            else
                alpha = 1.0f -
                    static_cast<float>((elapsed - HOLD) / FADE_OUT);

            alpha = std::clamp(alpha, 0.0f, 1.0f);

            const ImVec2 textSize =
                font->CalcTextSizeA(
                    FONT_SIZE,
                    FLT_MAX,
                    0.0f,
                    it->text.c_str());

            const float boxW = textSize.x + PAD_X * 2.0f;
            const float boxH = textSize.y + PAD_Y * 2.0f;

            const float slide =
                (elapsed < FADE_IN)
                ? (1.0f - alpha) * 40.0f
                : 0.0f;

            const float x0 = (dispW - boxW) * 0.5f;
            const float x1 = x0 + boxW;

            const float y0 = y - slide;
            const float y1 = y0 + boxH;

            const ImU32 bg =
                IM_COL32(
                    18,
                    19,
                    24,
                    static_cast<int>(160 * alpha));

            const ImU32 border =
                IM_COL32(
                    80,
                    90,
                    110,
                    static_cast<int>(140 * alpha));

            const ImU32 textColor =
                IM_COL32(
                    236,
                    239,
                    245,
                    static_cast<int>(255 * alpha));

            dl->AddRectFilled(
                ImVec2(x0, y0),
                ImVec2(x1, y1),
                bg,
                ROUNDING);

            dl->AddRect(
                ImVec2(x0, y0),
                ImVec2(x1, y1),
                border,
                ROUNDING,
                0,
                1.0f);

            dl->AddText(
                font,
                FONT_SIZE,
                ImVec2(
                    x0 + PAD_X,
                    y0 + PAD_Y),
                textColor,
                it->text.c_str());

            y = y1 + GAP;
        }
    }
}