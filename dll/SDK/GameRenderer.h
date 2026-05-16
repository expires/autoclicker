#pragma once
#include "Lunar.h"
#include "Camera.h"

class GameRenderer
{
public:
    GameRenderer(jobject instance) : instance(instance) {}

    jobject GetInstance() { return instance; }
    jclass  GetClass();

    Camera getMainCamera();
    // partialTicks usually 1.0f, useFOVSetting true. Return type is float in most
    // versions; if the mapping points at a double-returning overload, edit here.
    float  getFov(Camera cam, float partialTicks, bool useFOVSetting);

private:
    jobject instance;
};
