#pragma once
#include <Windows.h>
#include <atomic>

namespace LLMModule
{
    extern std::atomic<bool> running;
    DWORD WINAPI init(LPVOID lpParam);
}
