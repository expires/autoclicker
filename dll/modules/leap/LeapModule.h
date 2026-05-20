#pragma once
#include <Windows.h>

// Auto-leap exploit module — see Settings.h for the threat-model notes.
// Single attached JNI thread, polls at ~30Hz; fires synthetic right-clicks
// into MC's window when leap conditions hold (axe in hand + key held).
namespace LeapModule
{
    DWORD WINAPI init(LPVOID lpParam);
}
