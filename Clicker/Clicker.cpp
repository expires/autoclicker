#include "Clicker.h"

void Clicker::click(HWND hwnd)
{
    if (getClicksPerSecond() == 0) DELAY(100);
    if ((GetAsyncKeyState(VK_LBUTTON) >= 0)) return;
    DELAY(randomDelay())
    POINT pt;
    GetCursorPos(&pt);
    SendMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));
    DELAY(randomDelay())
    SendMessage(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(pt.x, pt.y));
    trackClick();
}

void Clicker::mouseDown(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);
    SendMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));
}

int Clicker::randomDelay()
{
    int interval = 500 / cps;
    std::uniform_int_distribution<> dis(interval - 10, interval + 10);
    return dis(gen);
}

int Clicker::getClicksPerSecond()
{
    auto now = std::chrono::steady_clock::now();
    clicks.erase(std::remove_if(clicks.begin(), clicks.end(),
        [now](const auto& time) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(now - time).count() > 1000;
        }), clicks.end());
    return static_cast<int>(clicks.size());
}

void Clicker::setCps(int cps_)
{
    cps = cps_;
}

void Clicker::trackClick(){
    clicks.push_back(std::chrono::steady_clock::now());
}