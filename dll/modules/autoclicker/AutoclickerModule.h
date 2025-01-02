#ifndef AutoclickerModule_H
#define AutoclickerModule_H

#include <Windows.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include "Clicker.h"
#include "../../SDK/Minecraft.h"

namespace AutoclickerModule
{
    inline constexpr extern double CPS = 16;
    inline constexpr extern int TICK = 50;
    extern Clicker clicker;
    extern std::atomic<bool> destruct;
    DWORD WINAPI init(const LPVOID lpParam);
}

#endif // AutoclickerModule_H