#pragma once
#include <Windows.h>
#include <string>
#include <vector>

namespace AntiEvadeModule
{
    struct DebugState
    {
        std::string target;
        bool        hovering  = false;
        bool        leather   = false;
        bool        usingItem = false;
        bool        sword     = false;
        bool        holding   = false;
        int         blockMs   = 0;
        int         tracked   = 0;
        int         chatLines = 0;
        int         ticks     = 0;
        std::string stage     = "not started";

        std::vector<std::string> events;
    };

    DWORD WINAPI init(LPVOID lpParam);

    bool ShouldHoldClicks();
    void NoteHit();
    void NoteSkip();

    DebugState Debug();
}
