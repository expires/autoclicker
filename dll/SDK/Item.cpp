#include "Item.h"

Item::Item(jobject instance)
{
    this->instance = instance;
}

jclass Item::GetClass()
{
    return lc->GetClass("net.minecraft.class_1792");
}

void Item::Cleanup()
{
    lc->env->DeleteLocalRef(this->instance);
}

jobject Item::GetInstance()
{
    return this->instance;
}

Component Item::getName(jobject itemStack)
{
    jmethodID name = lc->env->GetMethodID(this->GetClass(), "method_65043", "(Lnet/minecraft/class_1799;)Lnet/minecraft/class_2561;");

    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), name, itemStack);

    return Component(rtn);
}