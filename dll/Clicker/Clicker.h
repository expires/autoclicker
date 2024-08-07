#include <Windows.h>
#include <chrono>
#include <thread>
#include <random>

#define DELAY(x) std::this_thread::sleep_for(std::chrono::milliseconds(x));

class Clicker
{
public:
    Clicker() : gen(rd()), dis(50, 70), cps(12){};

    void click(HWND hwnd);
    void mouseDown(HWND hwnd);

    int randomDelay(int freq);
    int getClicksPerSecond();
    void setCps(int cps_);
private:
    int cps;
    std::vector<std::chrono::steady_clock::time_point> clicks;
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_int_distribution<> dis;
private:
    void trackClick();
};
