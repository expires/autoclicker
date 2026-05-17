#pragma once
#include <Windows.h>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace EspModule
{
    struct Target
    {
        double x,    y,    z;     // current-tick position
        double prevX, prevY, prevZ; // previous-tick position
        // AABB extents centered on the entity's feet position. Stored as
        // half-extents (and full height) so the overlay can rebuild the box
        // around the lerped position without needing to re-read the AABB.
        double halfWidth, height, halfDepth;
        // Team-formatted name decomposed into (text, ARGB) chunks. Each chunk
        // is one styled span — the overlay renders them side-by-side with
        // their per-chunk colors to match MC's vanilla nametag look.
        std::vector<std::pair<std::string, uint32_t>> nameChunks;
        // ImGui ABGR color for the ESP box, derived from the player's team
        // color so the box matches the nametag. Defaults to white if the
        // player has no team / no resolvable color.
        uint32_t boxColor = 0xFFFFFFFFu;
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
        // Diagnostic counters — exposed in the overlay so we can see where
        // the pipeline breaks when nothing renders.
        int  rawPlayerCount  = -1;  // -1 = level.players() call never reached
        bool gotMinecraft    = false;
        bool gotLocalPlayer  = false;
        bool gotLevel        = false;
        bool gotGameRenderer = false;
        bool gotCamera       = false;
    };

    // Acquire under lock, copy or read what you need, drop quickly.
    extern std::mutex   snapMutex;
    extern Snapshot     snapshot;

    DWORD WINAPI init(LPVOID lpParam);
}
