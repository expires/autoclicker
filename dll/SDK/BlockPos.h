#pragma once
#include "Lunar.h"

class BlockPos
{
public:
    explicit BlockPos(jobject instance) : instance(instance) {}

    jobject GetInstance() { return instance; }
    static jclass GetClass();

    static BlockPos make(int x, int y, int z);

private:
    jobject instance;
};
