#include "Clicker.h"
#include "AutoclickerModule.h"
#include "../sprintreset/SprintResetModule.h"

static long long steadyNowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

int Clicker::randomDelay(double fraction)
{
    const double cps = (cpsMax > cpsMin)
        ? cpsDist(gen, std::uniform_real_distribution<double>::param_type((double)cpsMin, (double)cpsMax))
        : (double)cpsMin;
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

bool Clicker::rollOneIn(int n)
{
    return chanceRoll(gen, std::uniform_int_distribution<>::param_type(1, n)) == 1;
}

void Clicker::updatePace()
{
    if (mode == 0) {
        paceFactor      = 0.0f;
        slowPhaseClicks = 0;
        slowMultiplier  = 1.0f;
        return;
    }

    paceFactor = paceFactor * 0.88f + paceImpulse(gen);
    if (paceFactor >  0.35f) paceFactor =  0.35f;
    if (paceFactor < -0.35f) paceFactor = -0.35f;

    if (slowPhaseClicks > 0) {
        if (--slowPhaseClicks == 0)
            slowMultiplier = 1.0f;
    } else if (rollOneIn(mode == 2 ? 15 : 50)) {
        slowPhaseClicks = slowDuration(gen);
        slowMultiplier  = static_cast<float>(slowFactor(gen));
    }
}

void Clicker::lclick(HWND hwnd, int jitterStrength, int hitType)
{
    if (getClicksPerSecond() == 0)
        DELAY(100);
    if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000))
        return;

    updatePace();

    const double downFrac = downFracDist(gen);

    POINT pt;
    GetCursorPos(&pt);
    SprintResetModule::PreClick(hitType == 2);
    SendMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));
    const int down = randomDelay(downFrac);
    armJitter(down, jitterStrength);
    DELAY(down);
    SendMessage(hwnd, WM_LBUTTONUP, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));
    SprintResetModule::PostClick();

    int gap = randomDelay(1.0 - downFrac);

    if (mode != 0 && rollOneIn(mode == 2 ? 25 : 90))
        gap = static_cast<int>(gap * pauseMult(gen));
    armJitter(gap, jitterStrength);
    DELAY(gap);

    trackClick();
}

void Clicker::invClick(HWND hwnd)
{
    updatePace();

    const double downFrac = downFracDist(gen);

    POINT pt;
    GetCursorPos(&pt);
    SendMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));
    DELAY(randomDelay(downFrac));
    SendMessage(hwnd, WM_LBUTTONUP, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));

    int gap = randomDelay(1.0 - downFrac);
    if (mode != 0 && rollOneIn(mode == 2 ? 25 : 90))
        gap = static_cast<int>(gap * pauseMult(gen));
    DELAY(gap);

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

void Clicker::armJitter(int totalMs, int strength)
{
    if (strength > 10) strength = 10;
    if (strength <= 0 || totalMs <= 0) {
        jitterLevel.store(0, std::memory_order_relaxed);
        return;
    }
    jitterLevel.store(strength, std::memory_order_relaxed);
    jitterUntil.store(steadyNowMs() + totalMs, std::memory_order_relaxed);
}

DWORD WINAPI Clicker::JitterWorker(LPVOID param)
{
    auto* self = static_cast<Clicker*>(param);

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> stepMs(6, 12);
    const float damping = 0.82f;

    while (!AutoclickerModule::destruct) {

        const int strength = self->jitterLevel.load(std::memory_order_relaxed);
        if (strength <= 0 || steadyNowMs() >= self->jitterUntil.load(std::memory_order_relaxed)) {
            self->jvx = self->jvy = 0.0f;
            self->jax = self->jay = 0.0f;
            DELAY(15);
            continue;
        }

        std::normal_distribution<float> impulse(0.0f, 0.16f * (float)strength);

        self->jvx = self->jvx * damping + impulse(rng);
        self->jvy = self->jvy * damping + impulse(rng);

        self->jax += self->jvx;
        self->jay += self->jvy;

        const int dx = (int)std::lround(self->jax);
        const int dy = (int)std::lround(self->jay);
        self->jax -= (float)dx;
        self->jay -= (float)dy;

        if (dx != 0 || dy != 0) {
            INPUT in = {};
            in.type         = INPUT_MOUSE;
            in.mi.dx        = dx;
            in.mi.dy        = dy;
            in.mi.dwFlags   = MOUSEEVENTF_MOVE;
            SendInput(1, &in, sizeof(INPUT));
        }

        DELAY(stepMs(rng));
    }
    return 0;
}
