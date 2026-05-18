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
    int menuKey = 0;
    int acKey   = 0;
    int espKey  = 0;

    // Hotbar macros — match an item by display name in slots 0-8, switch +
    // right-click, restore previous slot. Persisted alongside the rest of
    // the settings; a Macro with key=0 OR an empty name is treated as unset
    // by the macros thread.
    static constexpr int MAX_MACROS = 8;
    Macro macros[MAX_MACROS];

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
