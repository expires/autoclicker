#pragma once
#include <Windows.h>
#include <cstdio>

inline void McBotLogInit()
{
    AllocConsole();
    SetConsoleTitleA("MCBot Debug");
}

inline void McBotLog(const char* msg)
{
    HANDLE h = CreateFileA("CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;

    char buf[512];
    int  len = sprintf_s(buf, sizeof(buf), "[MCBot] %s\n", msg);
    DWORD written;
    WriteFile(h, buf, static_cast<DWORD>(len), &written, nullptr);
    CloseHandle(h);
}
