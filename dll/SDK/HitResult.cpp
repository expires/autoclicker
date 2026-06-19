#include "HitResult.h"
#include "Mappings.h"

HitResult::HitResult(jobject instance)
{
    this->instance = instance;
}

jclass HitResult::GetClass()
{
    static jclass c = nullptr;
    return JClass(c, MC_HitResult);
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
    static jmethodID hitType = nullptr;
    JMethod(hitType, this->GetClass(), MTD_HitResult_getType, DESC_HitResult_getType);
    jobject typeObj = lc->env->CallObjectMethod(this->GetInstance(), hitType);

    static jclass typeClass = nullptr;
    static jmethodID ordinalMethod = nullptr;
    if (!typeClass) JClass(typeClass, MC_HitResultType);
    JMethod(ordinalMethod, typeClass, "ordinal", "()I");
    return lc->env->CallIntMethod(typeObj, ordinalMethod);
}

EntityHitResult HitResult::getEntityHitResult()
{
    if (this->getType() != 2)
        return nullptr;

    return EntityHitResult(this->GetInstance());
}
