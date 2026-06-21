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

EntityHitResult HitResult::getEntityHitResult()
{
    if (this->getType() != 2)
        return nullptr;

    return EntityHitResult(this->GetInstance());
}
