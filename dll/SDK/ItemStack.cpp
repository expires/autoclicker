#include "ItemStack.h"

ItemStack::ItemStack(jobject instance)
{
    this->instance = instance;
}

jclass ItemStack::GetClass()
{
    return lc->GetClass("net.minecraft.world.item.ItemStack");
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
    jmethodID getItem = lc->env->GetMethodID(this->GetClass(), "getItem", "()Lnet/minecraft/world/item/Item;");

    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), getItem);

    return Item(rtn);
}