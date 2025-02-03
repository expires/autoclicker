#include "LivingEntity.h"

LivingEntity::LivingEntity(jobject instance)
{
    this->instance = instance;
}

jclass LivingEntity::GetClass()
{
    return lc->GetClass("net.minecraft.world.entity.LivingEntity");
}

void LivingEntity::Cleanup()
{
    lc->env->DeleteLocalRef(this->instance);
}

jobject LivingEntity::GetInstance()
{
    return this->instance;
}

bool LivingEntity::isUsingItem()
{
    jmethodID isUsingItem = lc->env->GetMethodID(this->GetClass(), "isUsingItem", "()Z");
    bool rtn = lc->env->CallBooleanMethod(this->GetInstance(), isUsingItem);

    return rtn;
}