#pragma once
#include "Lunar.h"

// Thin wrapper around net.minecraft.core.BlockPos (class_2338). MC's BlockPos
// is an immutable Vec3i — we construct one per lookup via the (int,int,int)
// constructor. Caller owns the local ref; release it (or PushLocalFrame
// around a hot loop) to keep the JNI table from growing.
class BlockPos
{
public:
    explicit BlockPos(jobject instance) : instance(instance) {}

    jobject GetInstance() { return instance; }
    static jclass GetClass();

    // Construct a fresh BlockPos local ref. Returns BlockPos(nullptr) on JNI
    // failure (class missing or constructor lookup failed). Callers should
    // null-check via GetInstance().
    static BlockPos make(int x, int y, int z);

private:
    jobject instance;
};
