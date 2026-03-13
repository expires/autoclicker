#include "EntityHitResult.h"

EntityHitResult::EntityHitResult(jobject instance)
{
    this->instance = instance;
}

jclass EntityHitResult::GetClass()
{
    return lc->GetClass("net.minecraft.class_3966");
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
    jmethodID getEntity = lc->env->GetMethodID(this->GetClass(), "method_17782", "()Lnet/minecraft/class_1297;");
    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), getEntity);

    return Entity(rtn);
}
