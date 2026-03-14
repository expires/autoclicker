#include "Level.h"
#include "Mappings.h"
#include "../Log.h"

Level::Level(jobject instance)
{
    this->instance = instance;
}

jclass Level::GetClass()
{
    return lc->GetClass(MC_Level);
}

void Level::Cleanup()
{
    lc->env->DeleteLocalRef(this->instance);
}

jobject Level::GetInstance()
{
    return this->instance;
}

jobject Level::players()
{
    jmethodID m = lc->env->GetMethodID(this->GetClass(),
        MTD_Level_players, DESC_Level_players);
    if (m == nullptr)
    {
        lc->env->ExceptionClear();
        McBotLog("Level::players: GetMethodID failed - check mtd_Level_players mapping");
        return nullptr;
    }
    jobject result = lc->env->CallObjectMethod(this->GetInstance(), m);
    if (lc->env->ExceptionCheck())
    {
        lc->env->ExceptionClear();
        McBotLog("Level::players: CallObjectMethod threw exception");
        return nullptr;
    }
    return result;
}
