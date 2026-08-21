#pragma once
#include <Windows.h>

namespace AntiEvadeModule
{
    DWORD WINAPI init(LPVOID lpParam);

    bool ShouldHoldClicks();
    void NoteHit();
}
