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

    float  getFov(Camera cam, float partialTicks, bool useFOVSetting);

private:
    jobject instance;
};
