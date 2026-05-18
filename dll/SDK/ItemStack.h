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

    // Display name including anvil rename. Falls back to the default item
    // name if the stack has no custom name. Returns Component(nullptr) on
    // any JNI failure — caller should check.
    Component getHoverName();

    // True for ItemStack.EMPTY (empty slot). Lets macros skip empty hotbar
    // slots without paying the cost of fetching a name.
    bool isEmpty();

private:
    jobject instance;
};
