#pragma once
#include "Lunar.h"
#include "ItemStack.h"

class Inventory
{
public:
    Inventory(jobject instance);

    jclass GetClass();

    jobject GetInstance();

    ItemStack getItem(int slot);

    int getSelected();

private:
    jobject instance;
};
