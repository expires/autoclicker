#include <Windows.h>
#include <chrono>
#include <thread>
#include <random>
#include <algorithm>
#include <cmath>

#define DELAY(x) std::this_thread::sleep_for(std::chrono::milliseconds(x));

class Clicker
{
public:
    Clicker(int cps) : cps(cps), gen(rd()),
        paceImpulse(0.0f, 0.028f),
        downFracDist(0.22, 0.38),
        pauseRoll(1, 25),
        pauseMult(1.6, 3.0),
        slowTrigger(1, 15),
        slowDuration(2, 5),
        slowFactor(1.25, 1.67),
        isMouseDown(false) {}
    void setCPS(int newCps) { cps = newCps; }

    // jitterStrength: 0 = plain click (no mouse motion), 1-10 = human-like
    // tremor injected during the click's hold + inter-click delays.
    void lclick(HWND hwnd, int jitterStrength = 0);
    void rclick(HWND hwnd);
    void mouseDown(HWND hwnd);
    int randomDelay(double fraction);
    int getClicksPerSecond();

private:
    int cps;
    bool isMouseDown;
    std::vector<std::chrono::steady_clock::time_point> clicks;
    std::random_device rd;
    std::mt19937 gen;

    // OU drift on effective click period (±35% max). Stepped once per click
    // so the mean CPS wanders over ~5-6 click half-life, producing the
    // time-varying variance that distinguishes human from bot patterns.
    float paceFactor = 0.0f;
    std::normal_distribution<float> paceImpulse;

    // Variable down-time fraction: U[0.22, 0.38] around the 0.30 mean.
    std::uniform_real_distribution<double> downFracDist;

    // Occasional human hesitation: 4% chance (roll==1) extends the gap
    // by 1.6x–3.0x, seeding right-tail outliers and positive kurtosis.
    std::uniform_int_distribution<> pauseRoll;
    std::uniform_real_distribution<double> pauseMult;

    // Discrete slow phase: ~6.7% chance per click (slowTrigger==1) of
    // entering a 2–5 click window at 20–40% reduced CPS. Breaks the
    // constant-rate signature that consistency checks look for.
    int slowPhaseClicks = 0;
    float slowMultiplier = 1.0f;
    std::uniform_int_distribution<> slowTrigger;
    std::uniform_int_distribution<> slowDuration;
    std::uniform_real_distribution<double> slowFactor;

    // Ornstein-Uhlenbeck state for the jitter random walk. Velocity is the
    // OU variable (random impulse + damping toward zero); sub-pixel position
    // accumulates so we can emit integer mouse deltas without quantization
    // losing the smaller motions.
    float jvx = 0.0f, jvy = 0.0f;
    float jax = 0.0f, jay = 0.0f;

    void trackClick();
    void updatePace();
    // Sleep for `totalMs` while running the OU jitter at the given strength.
    // strength == 0 is a plain DELAY — no motion, no per-step work.
    void jitterFor(int totalMs, int strength);
};
