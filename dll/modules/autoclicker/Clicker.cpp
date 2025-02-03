#include "Clicker.h"

int Clicker::randomDelay(int baseDelay)
{
    const double mean = baseDelay / cps;
    const double stddev = mean * 0.25;

    std::normal_distribution<> dis(mean, stddev);

    int delay;
    do {
        delay = static_cast<int>(dis(gen) * jitterFactor(gen));
    } while (delay < 1 || delay > mean * 2);

    if (highDelayChance(gen)) delay *= 1.5; 
    if (extremeDelayChance(gen) == 1) delay *= 2; 
   
    return delay;
}

void Clicker::lclick(HWND hwnd)
{
    if (getClicksPerSecond() == 0)
        DELAY(100);
    if ((GetAsyncKeyState(VK_LBUTTON) >= 0))
        return;

    POINT pt;
    GetCursorPos(&pt);
    SendMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));
    DELAY(randomDelay(400));
    SendMessage(hwnd, WM_LBUTTONUP, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));
    DELAY(randomDelay(550));

    trackClick();
}

void Clicker::rclick(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);
    SendMessage(hwnd, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(pt.x, pt.y));
    DELAY(randomDelay(500));
    SendMessage(hwnd, WM_RBUTTONUP, MK_RBUTTON, MAKELPARAM(pt.x, pt.y));

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

int64_t Clicker::jitter(HWND hwnd)
{
    const int minThreshold = 5;
    const int maxThreshold = 10;

    int totalDeltaX = (rand() % (maxThreshold - minThreshold + 1)) + minThreshold;
    int totalDeltaY = (rand() % (maxThreshold - minThreshold + 1)) + minThreshold;

    if (directionFlag)
    {
        totalDeltaX = abs(totalDeltaX);
        totalDeltaY = abs(totalDeltaY);
    }
    else
    {
        totalDeltaX = -abs(totalDeltaX);
        totalDeltaY = -abs(totalDeltaY);
    }

    int steps = max(abs(totalDeltaX), abs(totalDeltaY));

    float stepX = static_cast<float>(totalDeltaX) / steps;
    float stepY = static_cast<float>(totalDeltaY) / steps;

    float currentX = 0;
    float currentY = 0;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < steps; ++i)
    {
        currentX += stepX;
        currentY += stepY;

        INPUT input = {0};
        input.type = INPUT_MOUSE;
        input.mi.dx = static_cast<int>(currentX);
        input.mi.dy = static_cast<int>(currentY);
        input.mi.dwFlags = MOUSEEVENTF_MOVE;

        SendInput(1, &input, sizeof(INPUT));

        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        currentX -= static_cast<int>(currentX);
        currentY -= static_cast<int>(currentY);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    directionFlag = !directionFlag;

    return duration;
}
