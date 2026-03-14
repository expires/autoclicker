#pragma once
#include "Lunar.h"

class Level
{
public:
    Level(jobject instance);

    jclass GetClass();

    void Cleanup();

    jobject GetInstance();

    // Returns a java.util.List<Player> of all players in this level.
    // Caller must DeleteLocalRef the returned jobject when done.
    jobject players();

private:
    jobject instance;
};
