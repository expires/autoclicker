#include "HitResult.h"
#include "Mappings.h"

HitResult::HitResult(jobject instance)
{
    this->instance = instance;
}

jclass HitResult::GetClass()
{
    return lc->GetClass(MC_HitResult);
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
    jmethodID hitType = lc->env->GetMethodID(this->GetClass(), MTD_HitResult_getType, DESC_HitResult_getType);
    jobject typeObj = lc->env->CallObjectMethod(this->GetInstance(), hitType);

    jclass typeClass = lc->env->GetObjectClass(typeObj);
    jmethodID ordinalMethod = lc->env->GetMethodID(typeClass, "ordinal", "()I");
    return lc->env->CallIntMethod(typeObj, ordinalMethod);
}

EntityHitResult HitResult::getEntityHitResult()
{
    if (this->getType() != 2)
        return nullptr;

    return EntityHitResult(this->GetInstance());
}
