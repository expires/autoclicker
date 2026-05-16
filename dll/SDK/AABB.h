#pragma once
#include "Lunar.h"

class AABB
{
public:
    AABB(jobject instance) : instance(instance) {}

    jobject GetInstance() { return instance; }
    jclass  GetClass();

    double minX(); double minY(); double minZ();
    double maxX(); double maxY(); double maxZ();

private:
    jobject instance;
};
