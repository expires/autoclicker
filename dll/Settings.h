#pragma once

// One hotbar macro: when `key` is pressed (edge), the macros thread looks for
// an item whose display name contains `name` (case-insensitive substring) in
// slots 0-8, switches to that slot, right-clicks once, then restores the
// previously-held slot. `delay` is the millisecond pause between the slot
// switch and the right-click — gives MC time to register the new selection
// and sync with the server before the use-item fires.
struct Macro
{
    char name[64] = {};
    int  delay    = 100;
    int  key      = 0;
};

struct Settings
{
    bool acEnabled    = true;
    bool breakBlocks  = true;
    int  cps          = 10;
    bool selfDestruct = false;

    // Jitter mode: while autoclicking, push small organic mouse deltas
    // (Ornstein-Uhlenbeck random walk on velocity) so the cursor wanders
    // the way a tense human hand does instead of staying pixel-locked.
    // Strength is a 0-10 slider; 0 disables.
    bool jitterEnabled  = false;
    int  jitterStrength = 5;

    bool espEnabled   = true;
    bool drawBox      = true;
    bool drawName     = true;
    bool drawDistance = true;
    // 8 chunks (128 blocks). Fixed — no UI slider for this anymore.
    int  maxDistance  = 128;

    // Keybinds (VK_* codes). 0 = unbound.
    //   menuKey — falls back to VK_INSERT if 0 so the user can never lock
    //   themselves out of the menu by clearing the binding.
    //   acKey / espKey — 0 means "no toggle key", per the user's request
    //   to start with no ESP keybind.
    //   selfDestructKey — defaults to VK_END (0x23) so the historical hard-
    //   coded END behavior keeps working out of the box for users who don't
    //   touch it. Configurable in the Settings tab; pressing it sets
    //   selfDestruct = true, same as the in-menu Self-Destruct button.
    int menuKey         = 0;
    int acKey           = 0;
    int espKey          = 0;
    int selfDestructKey = 0x23; // VK_END

    // Hotbar macros — match an item by display name in slots 0-8, switch +
    // right-click, restore previous slot. Dynamic list: only entries
    // [0, macroCount) are active. The UI manages add/remove; the macros
    // thread only ever scans the active prefix.
    static constexpr int MAX_MACROS = 10;
    int   macroCount = 0;
    Macro macros[MAX_MACROS];

    // Aim assist — pushes raw mouse deltas (SendInput WM_INPUT) on top of the
    // user's own input each tick, nudging the crosshair toward the nearest
    // player within FOV+range. Skips same-team teammates (scoreboard team
    // ref-equality). Speeds are 0-10 ints; the module squares them so the
    // low end stays gentle. The aim point is the closest point on the
    // target's AABB to the current crosshair ray — so the assist stops
    // nudging the moment your view enters the hitbox.
    bool aimEnabled    = false;
    bool aimClickOnly  = true;
    int  aimSpeedH     = 5;
    int  aimSpeedV     = 5;
    int  aimFov        = 30;
    int  aimRange      = 6;
    int  aimKey        = 0;

    // Bump when defaults change. Load() force-resets the keybinds when it
    // reads an older version so legacy bindings (e.g. the historical
    // CapsLock→ESP that users had baked into their config) don't survive
    // the migration to "unbound by default".
    static constexpr int CURRENT_VERSION = 2;
    int version = CURRENT_VERSION;

    void Load();
    void Save();
};

inline Settings g_settings;
