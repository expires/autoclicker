#include "EntityHitResult.h"

EntityHitResult::EntityHitResult(jobject instance)
{
    this->instance = instance;
}

jclass EntityHitResult::GetClass()
{
    return lc->GetClass("net.minecraft.world.phys.EntityHitResult");
}

void EntityHitResult::Cleanup()
{
    lc->env->DeleteLocalRef(this->instance);
}

jobject EntityHitResult::GetInstance()
{
    return this->instance;
}

Entity EntityHitResult::getEntity()
{
    jmethodID getEntity = lc->env->GetMethodID(this->GetClass(), "getEntity", "()Lnet/minecraft/world/entity/Entity;");
    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), getEntity);

    return Entity(rtn);
}
