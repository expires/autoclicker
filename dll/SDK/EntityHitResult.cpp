#include "EntityHitResult.h"
#include "Mappings.h"

EntityHitResult::EntityHitResult(jobject instance)
{
    this->instance = instance;
}

jclass EntityHitResult::GetClass()
{
    return lc->GetClass(MC_EntityHitResult);
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
    jmethodID getEntity = lc->env->GetMethodID(this->GetClass(),
        MTD_EntityHitResult_getEntity, DESC_EntityHitResult_getEntity);
    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), getEntity);

    return Entity(rtn);
}
