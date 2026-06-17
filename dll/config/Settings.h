#pragma once

#include <mutex>
#include <string>
#include <vector>

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

    bool jitterEnabled  = false;
    int  jitterStrength = 5;

    bool espEnabled   = true;
    bool drawBox      = true;
    bool drawName     = true;
    bool drawDistance = true;
    bool drawHealth   = true;

    bool highlightFriends = true;

    bool teamsByColor = true;

    int  maxDistance  = 128;

    int menuKey         = 0xA1;
    int acKey           = 0;
    int espKey          = 0;
    int selfDestructKey = 0x23;

    static constexpr int MAX_MACROS = 10;
    int   macroCount = 0;
    Macro macros[MAX_MACROS];

    bool aimEnabled    = false;
    bool aimClickOnly  = true;
    int  aimSpeedH     = 5;
    int  aimSpeedV     = 5;
    int  aimFov        = 30;
    int  aimRange      = 6;
    int  aimKey        = 0;

    bool autoblockEnabled       = false;
    bool autoblockRequireSword  = true;
    int  autoblockDelay         = 100;
    int  autoblockCooldown      = 1000;
    int  autoblockKey           = 0;

    mutable std::mutex       friendsMutex;
    std::vector<std::string> friends;

    int friendKey = 0;

    static constexpr int CURRENT_VERSION = 3;
    int version = CURRENT_VERSION;

    void Load();
    void Save();
};

inline Settings g_settings;
