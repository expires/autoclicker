#include "EntityHitResult.h"
#include "Mappings.h"

Entity EntityHitResult::getEntity()
{
    static jmethodID getEntity = nullptr;
    JMethod(getEntity, this->GetClass(), MTD_EntityHitResult_getEntity, DESC_EntityHitResult_getEntity);
    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), getEntity);

    return Entity(rtn);
}
