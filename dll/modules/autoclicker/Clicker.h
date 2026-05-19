#include <Windows.h>
#include <chrono>
#include <thread>
#include <random>
#include <algorithm>

#define DELAY(x) std::this_thread::sleep_for(std::chrono::milliseconds(x));

class Clicker
{
public:
    Clicker(int cps) : cps(cps), gen(rd()), jitterFactor(0.9, 1.1), extremeDelayChance(1, 100), isMouseDown(false) {}
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
    std::uniform_real_distribution<> jitterFactor;
    std::uniform_int_distribution<> extremeDelayChance;

    // Ornstein-Uhlenbeck state for the jitter random walk. Velocity is the
    // OU variable (random impulse + damping toward zero); sub-pixel position
    // accumulates so we can emit integer mouse deltas without quantization
    // losing the smaller motions.
    float jvx = 0.0f, jvy = 0.0f;
    float jax = 0.0f, jay = 0.0f;

    void trackClick();
    // Sleep for `totalMs` while running the OU jitter at the given strength.
    // strength == 0 is a plain DELAY — no motion, no per-step work.
    void jitterFor(int totalMs, int strength);
};
