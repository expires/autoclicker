#pragma once
#include "Lunar.h"
#include <vector>

namespace Particles
{
    struct Point { double x, y, z; };

    bool Supported();

    void CollectSmoke(std::vector<Point>& out,
                      double camX, double camY, double camZ,
                      double maxDist, size_t cap);
}
