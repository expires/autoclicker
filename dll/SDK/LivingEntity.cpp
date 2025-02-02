#include "LivingEntity.h"

LivingEntity::LivingEntity(jobject instance)
{
    this->leInstance = instance;
}

jclass LivingEntity::GetClass()
{
    return lc->GetClass("net.minecraft.world.entity.LivingEntity");
}

void LivingEntity::Cleanup()
{
    lc->env->DeleteLocalRef(this->leInstance);
}

jobject LivingEntity::GetInstance()
{
    return this->leInstance;
}

bool LivingEntity::isUsingItem()
{
    jmethodID isUsingItem = lc->env->GetMethodID(this->GetClass(), "isUsingItem", "()Z");
    bool rtn = lc->env->CallBooleanMethod(this->GetInstance(), isUsingItem);

    return rtn;
}