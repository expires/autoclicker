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
};

inline Settings g_settings;
