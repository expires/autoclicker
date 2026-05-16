#pragma once
#include "Lunar.h"
#include "Player.h"
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

private:
    jobject instance;
};
