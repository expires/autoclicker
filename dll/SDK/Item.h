#pragma once
#include "Lunar.h"
#include "Component.h"

class Item
{
public:
    Item(jobject instance);

    jclass GetClass();

    void Cleanup();

    jobject GetInstance();

    Component getName(jobject itemStack);

private:
    jobject itemInstance;
};
