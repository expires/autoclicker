#pragma once
#include "Minecraft.h"
#include "Player.h"

struct ViewState
{
    double x = 0.0, y = 0.0, z = 0.0;
    float  yRot = 0.0f, xRot = 0.0f;
    float  fov = 70.0f;
    float  partialTick = 1.0f;
    bool   gotRenderer = false;
    bool   gotCamera   = false;
    bool   ok = false;
};

ViewState AcquireView(Minecraft& mc, Player& localPlayer);
