#include "EntityHitResult.h"
#include "Mappings.h"

EntityHitResult::EntityHitResult(jobject instance)
{
    this->instance = instance;
}

jclass EntityHitResult::GetClass()
{
    static jclass c = nullptr;
    return JClass(c, MC_EntityHitResult);
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
    static jmethodID getEntity = nullptr;
    JMethod(getEntity, this->GetClass(), MTD_EntityHitResult_getEntity, DESC_EntityHitResult_getEntity);
    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), getEntity);

    return Entity(rtn);
}
