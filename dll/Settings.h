#pragma once

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

    void Load();
    void Save();
};

inline Settings g_settings;
