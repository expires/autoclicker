#pragma once
#include "Lunar.h"

// Thin wrapper around net.minecraft.world.level.block.state.BlockState
// (class_2680). We only need the air check for the leap-cheat condition
// (server's wallKick rejects walls that are "airFoliage"; we approximate
// that as "not air" — foliage misclassifications produce false-positive
// fires, which the server-side cooldown then absorbs).
class BlockState
{
public:
    explicit BlockState(jobject instance) : instance(instance) {}

    jobject GetInstance() { return instance; }
    static jclass GetClass();

    // True if this state is air (passable, no collision). Returns true on
    // any JNI failure too — biases the caller toward "treat as air", which
    // for the leap module means "don't fire" (we need a wall behind to
    // fire). Better to skip a click than spuriously burn cooldown.
    bool isAir();

private:
    jobject instance;
};
