#pragma once
#include "Lunar.h"
#include "Vec3.h"

class Camera
{
public:
    Camera(jobject instance) : instance(instance) {}

    jobject GetInstance() { return instance; }
    jclass  GetClass();

    Vec3  getPosition();
    float getXRot();
    float getYRot();

private:
    jobject instance;
};
