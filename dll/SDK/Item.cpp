#include "Item.h"
#include "Mappings.h"

Item::Item(jobject instance)
{
    this->instance = instance;
}

jclass Item::GetClass()
{
    return lc->GetClass(MC_Item);
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
    jmethodID name = lc->env->GetMethodID(this->GetClass(),
        MTD_Item_getName, DESC_Item_getName);

    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), name, itemStack);

    return Component(rtn);
}
