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
