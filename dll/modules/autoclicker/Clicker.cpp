#include "Clicker.h"

// Log-normal delays with OU-based CPS drift. Intervals cluster around
// (1000/cps)*fraction but follow a right-skewed distribution: σ=0.18 gives
// CV≈18% and excess kurtosis≈+0.5, matching real human click timing.
// paceFactor shifts the effective mean ±35% on a ~5-click half-life so
// different time windows produce genuinely different variances.
int Clicker::randomDelay(double fraction)
{
    const double base  = (1000.0 / cps) * fraction;
    const double drift = 1.0 + (double)paceFactor;
    const double effective = base * (drift < 0.5 ? 0.5 : drift);

    const double sigma = 0.18;
    const double mu    = std::log(effective) - 0.5 * sigma * sigma;

    std::lognormal_distribution<double> dist(mu, sigma);
    const int delay = static_cast<int>(dist(gen));

    const int maxDelay = static_cast<int>(base * 4.5);
    return delay < 1 ? 1 : (delay > maxDelay ? maxDelay : delay);
}

void Clicker::updatePace()
{
    // OU step: damp toward zero then add Gaussian impulse.
    // 0.88^n = 0.5 → n ≈ 5.5 clicks half-life (≈460 ms at 12 CPS).
    paceFactor = paceFactor * 0.88f + paceImpulse(gen);
    if (paceFactor >  0.35f) paceFactor =  0.35f;
    if (paceFactor < -0.35f) paceFactor = -0.35f;
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
    // 4% chance of a human hesitation pause — extends the inter-click gap
    // by 1.6x–3.0x to seed right-tail outliers and positive excess kurtosis.
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

// Human-like tremor for `totalMs`. The old "sweep ±N pixels then reverse"
// approach was instantly identifiable as a script — pure diagonal lines,
// uniform speed, alternating directions. Real hand tremor is a damped
// random walk on velocity (Ornstein-Uhlenbeck): each step adds a small
// Gaussian impulse to the velocity, which is then dampened back toward
// zero, and position integrates the velocity. That produces continuous
// motion with no abrupt direction changes, mean-reverting so the cursor
// doesn't drift off-target over time, and a frequency content close to
// real physiological tremor (~8-12 Hz dominant).
//
// Strength 0 short-circuits to a plain DELAY — same wall time, no motion.
// Strength 10 produces visible jitter (~3-5 px excursions) without missing
// the target on typical PvP reach.
void Clicker::jitterFor(int totalMs, int strength)
{
    if (totalMs <= 0) return;
    if (strength <= 0) { DELAY(totalMs); return; }
    if (strength > 10) strength = 10;

    // Impulse scale: linear in strength. 0.08 px std per step at strength=1,
    // 0.8 at strength=10. Combined with the OU steady-state amplification
    // (1 / sqrt(1 - damping²) ≈ 1.9 at damping=0.18), peak excursion sits
    // around ~5 pixels at max strength — visible but not throwing aim off.
    const float impulseStd = 0.08f * (float)strength;
    const float damping    = 0.82f;  // velocity retention per step (1 - 0.18)

    std::normal_distribution<float>   impulse(0.0f, impulseStd);
    std::uniform_int_distribution<int> stepMs(6, 12);

    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(totalMs);

    while (std::chrono::steady_clock::now() < deadline) {
        // OU step on velocity.
        jvx = jvx * damping + impulse(gen);
        jvy = jvy * damping + impulse(gen);

        // Sub-pixel position accumulator so impulses below one pixel still
        // contribute over time instead of being lost to int truncation.
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

        // Varied step interval — uniform 5ms ticks would alias against the
        // server tick and produce a periodic-looking signature.
        DELAY(stepMs(gen));
    }
}
