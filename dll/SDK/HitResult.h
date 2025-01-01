#pragma once
#include "Lunar.h"
#include "BlockHitResult.h"

class HitResult
{
public:
    HitResult(jobject instance);

    jclass GetClass();

    void Cleanup();

    jobject GetInstance();

    int getType();

    BlockHitResult getBlockHitResult();

private:
    jobject hitResultInstance;
};
