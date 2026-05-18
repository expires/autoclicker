#include "Inventory.h"
#include "Mappings.h"

Inventory::Inventory(jobject instance)
{
    this->instance = instance;
}

jclass Inventory::GetClass()
{
    return lc->GetClass(MC_Inventory);
}

jobject Inventory::GetInstance()
{
    return this->instance;
}

ItemStack Inventory::getItem(int slot)
{
    jmethodID m = lc->env->GetMethodID(this->GetClass(),
        MTD_Inventory_getItem, DESC_Inventory_getItem);
    if (!m) { lc->env->ExceptionClear(); return ItemStack(nullptr); }
    jobject rtn = lc->env->CallObjectMethod(this->instance, m, (jint)slot);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return ItemStack(nullptr); }
    return ItemStack(rtn);
}

int Inventory::getSelected()
{
    jfieldID f = lc->env->GetFieldID(this->GetClass(),
        FLD_Inventory_selected, "I");
    if (!f) { lc->env->ExceptionClear(); return 0; }
    jint v = lc->env->GetIntField(this->instance, f);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return 0; }
    return (int)v;
}
