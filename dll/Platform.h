#pragma once
#include <Windows.h>

#ifndef MNC_LEGACY
#define MNC_LEGACY 0
#endif

#if MNC_LEGACY
#define MNC_GAME_WINDOW_CLASS L"LWJGL"
#else
#define MNC_GAME_WINDOW_CLASS L"GLFW30"
#endif

inline HWND FindGameWindow()
{
    HWND h = FindWindowW(MNC_GAME_WINDOW_CLASS, nullptr);
    if (h == nullptr)
        h = FindWindowW(MNC_LEGACY ? L"GLFW30" : L"LWJGL", nullptr);
    return h;
}
