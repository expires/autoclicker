#pragma once

struct Settings
{
    bool acEnabled    = true;
    bool breakBlocks  = true;
    int  cps          = 10;
    bool selfDestruct = false;

    bool espEnabled   = false;
    bool useGlow      = true;   // engine-rendered outline via setGlowingTag
    bool drawBox      = false;  // 2D AABB rect (jitters at no-interp; off by default)
    bool drawName     = false;
    bool drawDistance = false;
    int  maxDistance  = 100; // meters
};

inline Settings g_settings;
