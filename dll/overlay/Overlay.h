#pragma once

namespace Overlay
{
    void Init();
    void Shutdown();

    // True while the in-game menu is open. Read by MacrosModule so macros
    // don't fire while the user is typing in the overlay (e.g. editing a
    // macro name InputText).
    bool IsMenuVisible();
}
