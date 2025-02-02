#pragma once
#include "Lunar.h"

class LivingEntity
{
public:
    LivingEntity(jobject instance);

    jclass GetClass();

    void Cleanup();

    jobject GetInstance();

    bool isUsingItem();

private:
    jobject leInstance;
};
