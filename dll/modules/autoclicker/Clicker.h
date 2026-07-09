#pragma once
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <random>
#include <algorithm>
#include <cmath>
#include <vector>

#define DELAY(x) std::this_thread::sleep_for(std::chrono::milliseconds(x));

class Clicker
{
public:
    Clicker(int cps) : cpsMin(cps), cpsMax(cps), gen(rd()),
        paceImpulse(0.0f, 0.028f),
        downFracDist(0.22, 0.38),
        pauseMult(1.6, 3.0),
        slowDuration(2, 5),
        slowFactor(1.25, 1.67) {}
    void setCPS(int lo, int hi) { cpsMin = lo; cpsMax = hi < lo ? lo : hi; }
    void setMode(int m) { mode = m; }

    void lclick(HWND hwnd, int jitterStrength = 0, int hitType = -1);
    void invClick(HWND hwnd);
    void rclick(HWND hwnd);
    void mouseDown(HWND hwnd);
    int randomDelay(double fraction);
    int getClicksPerSecond();

    static DWORD WINAPI JitterWorker(LPVOID param);

private:
    int cpsMin;
    int cpsMax;
    int mode = 2;
    std::uniform_real_distribution<double> cpsDist;
    std::vector<std::chrono::steady_clock::time_point> clicks;
    std::random_device rd;
    std::mt19937 gen;

    float paceFactor = 0.0f;
    std::normal_distribution<float> paceImpulse;

    std::uniform_real_distribution<double> downFracDist;

    std::uniform_int_distribution<> chanceRoll;
    std::uniform_real_distribution<double> pauseMult;

    int slowPhaseClicks = 0;
    float slowMultiplier = 1.0f;
    std::uniform_int_distribution<> slowDuration;
    std::uniform_real_distribution<double> slowFactor;

    float jvx = 0.0f, jvy = 0.0f;
    float jax = 0.0f, jay = 0.0f;

    std::atomic<int>       jitterLevel{0};
    std::atomic<long long> jitterUntil{0};

    void trackClick();
    void updatePace();
    bool rollOneIn(int n);

    void armJitter(int totalMs, int strength);
};
