#include <Windows.h>
#include <chrono>
#include <thread>
#include <random>
#include <algorithm>

#define DELAY(x) std::this_thread::sleep_for(std::chrono::milliseconds(x));

class Clicker
{
public:
    Clicker(int cps) : cps(cps), gen(rd()), jitterFactor(0.9, 1.1), extremeDelayChance(1, 100), directionFlag(true), isMouseDown(false) {}
    void setCPS(int newCps) { cps = newCps; }
    void lclick(HWND hwnd);
    void rclick(HWND hwnd);
    void mouseDown(HWND hwnd);
    int randomDelay(double fraction);
    int getClicksPerSecond();

private:
    int cps;
    bool isMouseDown;
    bool directionFlag;
    std::vector<std::chrono::steady_clock::time_point> clicks;
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<> jitterFactor;
    std::uniform_int_distribution<> extremeDelayChance;
    void trackClick();
    int64_t jitter(HWND hwnd);
};
