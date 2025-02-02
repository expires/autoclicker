#include "Item.h"

Item::Item(jobject instance)
{
    this->itemInstance = instance;
}

jclass Item::GetClass()
{
    return lc->GetClass("net.minecraft.world.item.Item");
}

void Item::Cleanup()
{
    lc->env->DeleteLocalRef(this->itemInstance);
}

jobject Item::GetInstance()
{
    return this->itemInstance;
}

Component Item::getName(jobject itemStack)
{
    jmethodID name = lc->env->GetMethodID(this->GetClass(), "getName", "(Lnet/minecraft/world/item/ItemStack;)Lnet/minecraft/network/chat/Component;");

    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), name, itemStack);

    return Component(rtn);
}