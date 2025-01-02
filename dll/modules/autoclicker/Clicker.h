#include <Windows.h>
#include <chrono>
#include <thread>
#include <random>

#define DELAY(x) std::this_thread::sleep_for(std::chrono::milliseconds(x));

class Clicker
{
public:
    Clicker(int cps) : gen(rd()), dis(25, 55), cps(cps) {};
    void click(HWND hwnd, int delay);
    void mouseDown(HWND hwnd);
    int randomDelay(int freq);
    int getClicksPerSecond();

private:
    int cps;
    std::vector<std::chrono::steady_clock::time_point> clicks;
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_int_distribution<> dis;
    void trackClick();
};
