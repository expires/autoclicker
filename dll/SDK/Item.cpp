#include "Item.h"
#include "Mappings.h"

Item::Item(jobject instance)
{
    this->instance = instance;
}

jclass Item::GetClass()
{
    static jclass c = nullptr;
    return JClass(c, MC_Item);
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
    static jmethodID name = nullptr;
    JMethod(name, this->GetClass(), MTD_Item_getName, DESC_Item_getName);

    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), name, itemStack);

    return Component(rtn);
}
