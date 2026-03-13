#pragma once

struct Settings
{
    bool acEnabled   = true;
    bool breakBlocks = true;
    int  cps         = 10;
};

inline Settings g_settings;
