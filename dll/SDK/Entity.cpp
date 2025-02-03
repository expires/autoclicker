#include "Entity.h"

Entity::Entity(jobject instance)
{
    this->instance = instance;
}

jclass Entity::GetClass()
{
    return lc->GetClass("net.minecraft.world.entity.Entity");
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
    jmethodID name = lc->env->GetMethodID(this->GetClass(), "getName", "()Lnet/minecraft/network/chat/Component;");

    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), name);

    return Component(rtn);
}

Component Entity::getTypeName()
{
    jmethodID typeName = lc->env->GetMethodID(this->GetClass(), "getTypeName", "()Lnet/minecraft/network/chat/Component;");

    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), typeName);

    return Component(rtn);
}