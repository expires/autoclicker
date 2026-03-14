#pragma once
#include <Windows.h>
#include <cstdio>

inline void McBotLogInit()
{
    AllocConsole();
    SetConsoleTitleA("MCBot Debug");
    FILE* dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout);
}

inline void McBotLog(const char* msg)
{
    printf("[MCBot] %s\n", msg);
}
