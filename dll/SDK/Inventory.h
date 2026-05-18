#pragma once
#include "Lunar.h"
#include "ItemStack.h"

// Thin wrapper around net.minecraft.world.entity.player.Inventory. Used by the
// macros module to read hotbar contents (slots 0-8) by display name and to
// read which slot the player currently has selected (for restore-after-fire).
class Inventory
{
public:
    Inventory(jobject instance);

    jclass GetClass();

    jobject GetInstance();

    // Returns the ItemStack at the given inventory slot. Slots 0-8 are the
    // hotbar. Returns a wrapper around ItemStack.EMPTY if the slot is empty
    // (caller should check isEmpty()).
    ItemStack getItem(int slot);

    // Index of the currently held hotbar slot (0-8). The macros module reads
    // this before switching so it can restore the player's previous selection
    // after firing.
    int getSelected();

private:
    jobject instance;
};
