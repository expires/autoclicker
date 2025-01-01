#pragma once
#include "Lunar.h"
#include "BlockPos.h"

class BlockHitResult
{
public:
    BlockHitResult(jobject instance);

    jclass GetClass();

    void Cleanup();

    jobject GetInstance();

    BlockPos getBlockPos();

private:
    jobject blockHitResultInstance;
};
