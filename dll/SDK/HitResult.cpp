#include "HitResult.h"
#include <iostream>

HitResult::HitResult(jobject instance)
{
    this->instance = instance;
}

jclass HitResult::GetClass()
{
    return lc->GetClass("net.minecraft.world.phys.HitResult");
}

void HitResult::Cleanup()
{
    lc->env->DeleteLocalRef(this->instance);
}

jobject HitResult::GetInstance()
{
    return this->instance;
}

int HitResult::getType()
{
    jmethodID hitType = lc->env->GetMethodID(this->GetClass(), "getType", "()Lnet/minecraft/world/phys/HitResult$Type;");
    jobject typeObj = lc->env->CallObjectMethod(this->GetInstance(), hitType);

    jclass typeClass = lc->env->GetObjectClass(typeObj);
    jmethodID ordinalMethod = lc->env->GetMethodID(typeClass, "ordinal", "()I");
    jint rtn = lc->env->CallIntMethod(typeObj, ordinalMethod);

    return rtn;
}

EntityHitResult HitResult::getEntityHitResult()
{
    if (this->getType() != 2)
        return nullptr;

    jclass entityHitResultClass = lc->env->FindClass("net/minecraft/world/phys/EntityHitResult");
    return EntityHitResult(this->GetInstance());
}