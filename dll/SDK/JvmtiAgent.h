#pragma once
#include <jvmti.h>

namespace JvmtiAgent
{
    // Negotiates JVMTI capabilities, installs the Breakpoint callback, and sets
    // a breakpoint at instruction 0 of AvatarRenderer.shouldShowName. From then
    // on, every time MC asks "should I draw the nametag for this player", our
    // callback fires; if g_settings.espEnabled && g_settings.drawName is true
    // we ForceEarlyReturn(false), suppressing MC's vanilla nametag rendering.
    // Returns true on success; logs (via printf, currently a no-op in Lunar)
    // on each failure path.
    bool Init();

    // Disables the Breakpoint event and clears the breakpoint. Safe to call
    // even if Init() failed.
    void Shutdown();

    // True once SetBreakpoint succeeded. Exposed for the overlay diagnostic so
    // we can see at a glance whether the hook is installed (printf goes nowhere
    // in Lunar's no-console environment).
    bool IsActive();
}
