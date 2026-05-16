#pragma once
#include <Windows.h>
#include <mutex>
#include <string>
#include <vector>

namespace EspModule
{
    struct Target
    {
        double x, y, z;
        double minX, minY, minZ;
        double maxX, maxY, maxZ;
        std::string name;
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
        bool                valid = false;
        // Diagnostic counters — exposed in the overlay so we can see where
        // the pipeline breaks when nothing renders.
        int  rawPlayerCount  = -1;  // -1 = level.players() call never reached
        bool gotMinecraft    = false;
        bool gotLocalPlayer  = false;
        bool gotLevel        = false;
        bool gotGameRenderer = false;
        bool gotCamera       = false;
        int  glowCallsOk     = 0;   // setGlowingTag(true) calls that succeeded
        int  glowCallsFail   = 0;   // setGlowingTag(true) calls that failed
    };

    // Acquire under lock, copy or read what you need, drop quickly.
    extern std::mutex   snapMutex;
    extern Snapshot     snapshot;

    DWORD WINAPI init(LPVOID lpParam);
}
