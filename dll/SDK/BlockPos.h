#pragma once
#include "Lunar.h"

class BlockPos
{
public:
    BlockPos(jobject instance);

    jclass GetClass();

    void Cleanup();

    jobject GetInstance();

private:
    jobject blockPosInstance;
};
