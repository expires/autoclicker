#pragma once
#include "Lunar.h"
#include "Entity.h"

class EntityHitResult
{
public:
    EntityHitResult(jobject instance);

    jclass GetClass();

    void Cleanup();

    jobject GetInstance();

    Entity getEntity();

private:
    jobject instance;
};
