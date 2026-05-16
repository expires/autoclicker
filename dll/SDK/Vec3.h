#pragma once
#include "Lunar.h"

class Vec3
{
public:
    Vec3(jobject instance) : instance(instance) {}

    jobject GetInstance() { return instance; }
    jclass  GetClass();

    double getX();
    double getY();
    double getZ();

private:
    jobject instance;
};
