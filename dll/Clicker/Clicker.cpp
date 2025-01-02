#include "Clicker.h"

int Clicker::randomDelay(int baseDelay)
{
    const int interval = baseDelay / cps;
    const int minDelay = interval - 10;
    const int maxDelay = interval + 10;

    std::uniform_int_distribution<> dis(minDelay, maxDelay);
    return dis(gen);
}

void Clicker::click(HWND hwnd)
{
    int cps = getClicksPerSecond();
    if (cps == 0)
        DELAY(150);
    if ((GetAsyncKeyState(VK_LBUTTON) >= 0))
        return;
    POINT pt;
    GetCursorPos(&pt);

    SendMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));
    DELAY(randomDelay(500));
    SendMessage(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(pt.x, pt.y));

    trackClick();
}

void Clicker::mouseDown(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);
    SendMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));
}

int Clicker::getClicksPerSecond()
{
    auto now = std::chrono::steady_clock::now();
    clicks.erase(std::remove_if(clicks.begin(), clicks.end(),
                                [now](const auto &time)
                                {
                                    return std::chrono::duration_cast<std::chrono::milliseconds>(now - time).count() > 1000;
                                }),
                 clicks.end());
    return static_cast<int>(clicks.size());
}

void Clicker::trackClick()
{
    clicks.push_back(std::chrono::steady_clock::now());
}