#pragma once
#include <Windows.h>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace EspModule
{
    struct Target
    {
        double x, y, z;
        double prevX, prevY, prevZ;
        double halfWidth, height, halfDepth;
        std::vector<std::pair<std::string, uint32_t>> nameChunks;
        uint32_t boxColor = 0xFFFFFFFFu;
        std::string playerName;
        bool        isFriend = false;
        float       health    = -1.0f;
        float       maxHealth = -1.0f;
    };

    struct CameraState
    {
        double x, y, z;
        float  yRot, xRot;
        float  fov;
    };

    struct Snapshot
    {
        std::vector<Target> targets;
        CameraState         cam{};
        float               partialTick = 1.0f;
        bool                valid = false;
        int  rawPlayerCount  = -1;
        bool gotMinecraft    = false;
        bool gotLocalPlayer  = false;
        bool gotLevel        = false;
        bool gotGameRenderer = false;
        bool gotCamera       = false;
    };

    std::shared_ptr<const Snapshot> Acquire();

    DWORD WINAPI init(LPVOID lpParam);
}
