#pragma once
#include "Lunar.h"
#include "Player.h"
#include "BlockPos.h"
#include "BlockState.h"
#include <vector>

class Level
{
public:
    Level(jobject instance) : instance(instance) {}

    jobject GetInstance() { return instance; }
    jclass  GetClass();

    // Returns each entry as a Player wrapper around a local ref. Caller owns the
    // refs — release them (or PushLocalFrame around the whole snapshot loop).
    std::vector<Player> players();

    // BlockGetter.getBlockState(BlockPos) — Level extends LevelReader extends
    // BlockGetter, so this resolves against ClientLevel. Returns
    // BlockState(nullptr) on JNI failure; BlockState::isAir treats null as
    // "air" so a failed read biases toward "no wall" (safer for the leap
    // cheat: skip the click rather than fire blindly).
    BlockState getBlockState(BlockPos& pos);

private:
    jobject instance;
};
