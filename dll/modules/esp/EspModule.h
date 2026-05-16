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
    };

    // Acquire under lock, copy or read what you need, drop quickly.
    extern std::mutex   snapMutex;
    extern Snapshot     snapshot;

    DWORD WINAPI init(LPVOID lpParam);
}
