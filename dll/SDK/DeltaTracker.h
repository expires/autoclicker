#pragma once
#include "Lunar.h"

class DeltaTracker
{
public:
    DeltaTracker(jobject instance) : instance(instance) {}

    jobject GetInstance() { return instance; }
    jclass  GetClass();

    float getPartialTick(bool runsNormally);

private:
    jobject instance;
};
