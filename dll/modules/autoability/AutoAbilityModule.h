#pragma once
#include <Windows.h>

// Auto-ability module — fires synthetic right-clicks at the MC window when
// the player is holding a sword and the crosshair is on a living entity.
// Built as a testbed for cooldown/spam handling in the user's anticheat;
// `delay` rate-limits the attempt cadence, `cooldown` enforces a minimum
// gap between successful fires (matches the typical server-side ability
// cooldown so we don't waste attempts on rejected presses).
namespace AutoAbilityModule
{
    DWORD WINAPI init(LPVOID lpParam);
}
