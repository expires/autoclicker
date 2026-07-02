#pragma once
#include <Windows.h>
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
    Clicker(int cps) : cps(cps), gen(rd()),
        paceImpulse(0.0f, 0.028f),
        downFracDist(0.22, 0.38),
        pauseRoll(1, 25),
        pauseMult(1.6, 3.0),
        slowTrigger(1, 15),
        slowDuration(2, 5),
        slowFactor(1.25, 1.67) {}
    void setCPS(int newCps) { cps = newCps; }

    void lclick(HWND hwnd, int jitterStrength = 0, int hitType = -1);
    void invClick(HWND hwnd);
    void rclick(HWND hwnd);
    void mouseDown(HWND hwnd);
    int randomDelay(double fraction);
    int getClicksPerSecond();

private:
    int cps;
    std::vector<std::chrono::steady_clock::time_point> clicks;
    std::random_device rd;
    std::mt19937 gen;

    float paceFactor = 0.0f;
    std::normal_distribution<float> paceImpulse;

    std::uniform_real_distribution<double> downFracDist;

    std::uniform_int_distribution<> pauseRoll;
    std::uniform_real_distribution<double> pauseMult;

    int slowPhaseClicks = 0;
    float slowMultiplier = 1.0f;
    std::uniform_int_distribution<> slowTrigger;
    std::uniform_int_distribution<> slowDuration;
    std::uniform_real_distribution<double> slowFactor;

    float jvx = 0.0f, jvy = 0.0f;
    float jax = 0.0f, jay = 0.0f;

    void trackClick();
    void updatePace();

    void jitterFor(int totalMs, int strength);
};
