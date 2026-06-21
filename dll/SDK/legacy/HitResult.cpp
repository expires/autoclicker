#include "HitResult.h"
#include "Mappings.h"

int HitResult::getType()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_HitResult_typeOfHit, DESC_HitResult_typeOfHit)) return 0;
    jobject typeObj = lc->env->GetObjectField(this->GetInstance(), f);
    if (!typeObj || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return 0; }

    static jclass typeClass = nullptr;
    static jmethodID ordinalMethod = nullptr;
    if (!typeClass) JClass(typeClass, MC_HitResultType);
    JMethod(ordinalMethod, typeClass, "ordinal", "()I");
    return lc->env->CallIntMethod(typeObj, ordinalMethod);
}
