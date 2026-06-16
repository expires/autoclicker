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

    std::vector<Player> players();

    BlockState getBlockState(BlockPos& pos);

private:
    jobject instance;
};
