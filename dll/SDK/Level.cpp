#include "Level.h"
#include "Mappings.h"

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
    return lc->env->CallObjectMethod(this->GetInstance(), m);
}
