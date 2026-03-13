#include "Entity.h"

Entity::Entity(jobject instance)
{
    this->instance = instance;
}

jclass Entity::GetClass()
{
    return lc->GetClass("net.minecraft.class_1297");
}

void Entity::Cleanup()
{
    lc->env->DeleteLocalRef(this->instance);
}

jobject Entity::GetInstance()
{
    return this->instance;
}

Component Entity::getName()
{
    jmethodID name = lc->env->GetMethodID(this->GetClass(), "method_5477", "()Lnet/minecraft/class_2561;");

    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), name);

    return Component(rtn);
}

Component Entity::getTypeName()
{
    jmethodID typeName = lc->env->GetMethodID(this->GetClass(), "method_23315", "()Lnet/minecraft/class_2561;");

    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), typeName);

    return Component(rtn);
}