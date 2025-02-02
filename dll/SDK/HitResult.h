#pragma once
#include "Lunar.h"
#include "EntityHitResult.h"

class HitResult
{
public:
    HitResult(jobject instance);

    jclass GetClass();

    void Cleanup();

    jobject GetInstance();

    int getType();

    EntityHitResult getEntityHitResult();

private:
    jobject hitResultInstance;
};
