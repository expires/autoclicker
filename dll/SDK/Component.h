#pragma once
#include "Lunar.h"

class Component
{
public:
    Component(jobject instance);

    jclass GetClass();

    void Cleanup();

    jobject GetInstance();

    std::string getString();

private:
    jobject instance;
};
