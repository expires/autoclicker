#pragma once

namespace ScaffoldModule
{
    // Runs on the render thread (Minecraft's main thread), so world reads are
    // synchronised with the game and never race a block placement.
    void Tick();

    // Releases any held sneak; call on teardown.
    void Release();
}
