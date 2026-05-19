#ifndef AimAssistModule_H
#define AimAssistModule_H

#include <Windows.h>

namespace AimAssistModule
{
    // JNI-attached worker thread. Each tick: find the player nearest to the
    // crosshair (within FOV cone + range, skipping the same-team scoreboard
    // group), compute the yaw/pitch delta needed to aim at them, and inject
    // a raw mouse delta via SendInput. The delta is additive to whatever the
    // user is doing — GLFW's raw-input listener sees the OS-level sum.
    DWORD WINAPI init(LPVOID lpParam);
}

#endif
