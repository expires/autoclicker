#pragma once
#include "Lunar.h"
#include "Item.h"
#include <string>

class ItemStack
{
public:
    ItemStack(jobject instance);

    jclass GetClass();

    void Cleanup();

    jobject GetInstance();

    Item getItem();

    Component getHoverName();

    std::string getDescriptionId();

    bool isEmpty();

private:
    jobject instance;
};
