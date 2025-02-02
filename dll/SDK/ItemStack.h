#pragma once
#include "Lunar.h"
#include "Item.h"

class ItemStack
{
public:
    ItemStack(jobject instance);

    jclass GetClass();

    void Cleanup();

    jobject GetInstance();

    Item getItem();

private:
    jobject isInstance;
};
