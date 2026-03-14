#pragma once
#include <Windows.h>
#include <cstdio>

// Appends a line to mcbot_debug.log in the same directory as the DLL.
// Safe to call from any thread.
inline void McBotLog(const char* msg)
{
    static char s_path[MAX_PATH] = {};
    if (s_path[0] == '\0')
    {
        HMODULE hm = nullptr;
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&McBotLog), &hm);
        GetModuleFileNameA(hm, s_path, MAX_PATH);
        char* slash = strrchr(s_path, '\\');
        if (slash) *(slash + 1) = '\0';
        strcat_s(s_path, "mcbot_debug.log");
    }

    FILE* f = nullptr;
    fopen_s(&f, s_path, "a");
    if (f)
    {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}
