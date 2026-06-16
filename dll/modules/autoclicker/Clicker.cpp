#include "Clicker.h"

int Clicker::randomDelay(double fraction)
{
    const double base  = (1000.0 / cps) * fraction;
    const double drift = 1.0 + (double)paceFactor;
    const double effective = base * (drift < 0.5 ? 0.5 : drift) * (double)slowMultiplier;

    const double sigma = 0.18;
    const double mu    = std::log(effective) - 0.5 * sigma * sigma;

    std::lognormal_distribution<double> dist(mu, sigma);
    const int delay = static_cast<int>(dist(gen));

    const int maxDelay = static_cast<int>(base * 4.5);
    return delay < 1 ? 1 : (delay > maxDelay ? maxDelay : delay);
}

void Clicker::updatePace()
{

    paceFactor = paceFactor * 0.88f + paceImpulse(gen);
    if (paceFactor >  0.35f) paceFactor =  0.35f;
    if (paceFactor < -0.35f) paceFactor = -0.35f;

    if (slowPhaseClicks > 0) {
        if (--slowPhaseClicks == 0)
            slowMultiplier = 1.0f;
    } else if (slowTrigger(gen) == 1) {
        slowPhaseClicks = slowDuration(gen);
        slowMultiplier  = static_cast<float>(slowFactor(gen));
    }
}

void Clicker::lclick(HWND hwnd, int jitterStrength)
{
    if (getClicksPerSecond() == 0)
        DELAY(100);
    if (GetAsyncKeyState(VK_LBUTTON) >= 0)
        return;

    updatePace();

    const double downFrac = downFracDist(gen);

    POINT pt;
    GetCursorPos(&pt);
    SendMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));
    jitterFor(randomDelay(downFrac), jitterStrength);
    SendMessage(hwnd, WM_LBUTTONUP, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));

    int gap = randomDelay(1.0 - downFrac);

    if (pauseRoll(gen) == 1)
        gap = static_cast<int>(gap * pauseMult(gen));
    jitterFor(gap, jitterStrength);

    trackClick();
}

void Clicker::rclick(HWND hwnd)
{
    updatePace();

    const double downFrac = downFracDist(gen);

    POINT pt;
    GetCursorPos(&pt);
    SendMessage(hwnd, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(pt.x, pt.y));
    DELAY(randomDelay(downFrac));
    SendMessage(hwnd, WM_RBUTTONUP, MK_RBUTTON, MAKELPARAM(pt.x, pt.y));
    DELAY(randomDelay(1.0 - downFrac));

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

void Clicker::jitterFor(int totalMs, int strength)
{
    if (totalMs <= 0) return;
    if (strength <= 0) { DELAY(totalMs); return; }
    if (strength > 10) strength = 10;

    const float impulseStd = 0.08f * (float)strength;
    const float damping    = 0.82f;

    std::normal_distribution<float>   impulse(0.0f, impulseStd);
    std::uniform_int_distribution<int> stepMs(6, 12);

    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(totalMs);

    while (std::chrono::steady_clock::now() < deadline) {

        jvx = jvx * damping + impulse(gen);
        jvy = jvy * damping + impulse(gen);

        jax += jvx;
        jay += jvy;

        const int dx = (int)std::lround(jax);
        const int dy = (int)std::lround(jay);
        jax -= (float)dx;
        jay -= (float)dy;

        if (dx != 0 || dy != 0) {
            INPUT in = {};
            in.type         = INPUT_MOUSE;
            in.mi.dx        = dx;
            in.mi.dy        = dy;
            in.mi.dwFlags   = MOUSEEVENTF_MOVE;
            SendInput(1, &in, sizeof(INPUT));
        }

        DELAY(stepMs(gen));
    }
}
