#include "HitResult.h"
#include <iostream>

HitResult::HitResult(jobject instance)
{
    this->instance = instance;
}

jclass HitResult::GetClass()
{
    return lc->GetClass("net.minecraft.class_239");
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
    jmethodID hitType = lc->env->GetMethodID(this->GetClass(), "method_17783", "()Lnet/minecraft/class_239$class_240;");
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

    return EntityHitResult(this->GetInstance());
}