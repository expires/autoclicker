#pragma once

struct Settings
{
    bool acEnabled    = true;
    bool breakBlocks  = true;
    int  cps          = 10;
    bool selfDestruct = false;

    bool espEnabled   = false;
    bool useGlow      = false;  // engine outline; Lunar's render path ignores it
    bool drawBox      = true;
    bool drawName     = false;  // off by default — MC's vanilla nametag already shows
    bool drawDistance = true;
    int  maxDistance  = 100; // meters
};

inline Settings g_settings;
