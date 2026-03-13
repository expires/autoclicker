#include "ItemStack.h"
#include "Mappings.h"

ItemStack::ItemStack(jobject instance)
{
    this->instance = instance;
}

jclass ItemStack::GetClass()
{
    return lc->GetClass(MC_ItemStack);
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
    jmethodID getItem = lc->env->GetMethodID(this->GetClass(),
        MTD_ItemStack_getItem, DESC_ItemStack_getItem);

    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), getItem);

    return Item(rtn);
}
