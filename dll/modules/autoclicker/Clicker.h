#include <Windows.h>
#include <chrono>
#include <thread>
#include <random>
#include <algorithm>

#define DELAY(x) std::this_thread::sleep_for(std::chrono::milliseconds(x));

class Clicker
{
public:
    Clicker(int cps) : gen(rd()), dis(25, 55), cps(cps), isMouseDown(false), highDelayChance(0.25), jitterFactor(0.9, 1.1), extremeDelayChance(1, 100) {};
    void lclick(HWND hwnd);
    void rclick(HWND hwnd);
    void mouseDown(HWND hwnd);
    int randomDelay(int freq);
    int getClicksPerSecond();

private:
    int cps;
    bool isMouseDown;
    bool directionFlag = true;
    std::vector<std::chrono::steady_clock::time_point> clicks;
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_int_distribution<> dis;
    std::bernoulli_distribution highDelayChance;
    std::uniform_real_distribution<> jitterFactor;
    std::uniform_int_distribution<> extremeDelayChance;
    void trackClick();
    int64_t jitter(HWND hwnd);
};
