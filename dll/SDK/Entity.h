#pragma once
#include "Lunar.h"
#include "Component.h"

class Entity
{
public:
    Entity(jobject instance);

    virtual jclass GetClass();

    void Cleanup();

    jobject GetInstance();

    Component getName();

    Component getTypeName();

private:
    jobject instance;
};
