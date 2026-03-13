#include "ItemStack.h"

ItemStack::ItemStack(jobject instance)
{
    this->instance = instance;
}

jclass ItemStack::GetClass()
{
    return lc->GetClass("net.minecraft.class_1799");
}

void ItemStack::Cleanup()
{
    lc->env->DeleteLocalRef(this->instance);
}

jobject ItemStack::GetInstance()
{
    return this->instance;
}

Item ItemStack::getItem()
{
    jmethodID getItem = lc->env->GetMethodID(this->GetClass(), "method_57385", "()Lnet/minecraft/class_1792;");

    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), getItem);

    return Item(rtn);
}