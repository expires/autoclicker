#include "EntityHitResult.h"
#include "Mappings.h"

Entity EntityHitResult::getEntity()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_HitResult_entityHit, DESC_HitResult_entityHit)) return Entity(nullptr);
    jobject rtn = lc->env->GetObjectField(this->GetInstance(), f);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return Entity(nullptr); }
    return Entity(rtn);
}
