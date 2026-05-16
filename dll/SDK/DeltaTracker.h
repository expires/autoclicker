#pragma once
#include "Lunar.h"

class DeltaTracker
{
public:
    DeltaTracker(jobject instance) : instance(instance) {}

    jobject GetInstance() { return instance; }
    jclass  GetClass();

    // MC's render-time partial tick. Passing true returns the value that
    // tracks normal game ticks (matches what EntityRenderer uses).
    float getPartialTick(bool runsNormally);

private:
    jobject instance;
};
