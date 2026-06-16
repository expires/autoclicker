#pragma once
#include "Lunar.h"

class BlockState
{
public:
    explicit BlockState(jobject instance) : instance(instance) {}

    jobject GetInstance() { return instance; }
    static jclass GetClass();

    bool isAir();

    bool blocksMotion();

private:
    jobject instance;
};
