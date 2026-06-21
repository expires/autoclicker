#include "HitResult.h"
#include "Mappings.h"

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
