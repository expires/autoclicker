#pragma once
#include <Windows.h>

#ifndef AC_LEGACY
#define AC_LEGACY 0
#endif

#if AC_LEGACY
#define AC_GAME_WINDOW_CLASS L"LWJGL"
#else
#define AC_GAME_WINDOW_CLASS L"GLFW30"
#endif

inline HWND FindGameWindow()
{
    HWND h = FindWindowW(AC_GAME_WINDOW_CLASS, nullptr);
    if (h == nullptr)
        h = FindWindowW(AC_LEGACY ? L"GLFW30" : L"LWJGL", nullptr);
    return h;
}
