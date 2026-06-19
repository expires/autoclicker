#include "Inventory.h"
#include "Mappings.h"

Inventory::Inventory(jobject instance)
{
    this->instance = instance;
}

jclass Inventory::GetClass()
{
    static jclass c = nullptr;
    return JClass(c, MC_Inventory);
}

jobject Inventory::GetInstance()
{
    return this->instance;
}

ItemStack Inventory::getItem(int slot)
{
    static jmethodID m = nullptr;
    JMethod(m, this->GetClass(), MTD_Inventory_getItem, DESC_Inventory_getItem);
    if (!m) { lc->env->ExceptionClear(); return ItemStack(nullptr); }
    jobject rtn = lc->env->CallObjectMethod(this->instance, m, (jint)slot);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return ItemStack(nullptr); }
    return ItemStack(rtn);
}

int Inventory::getSelected()
{
    static jfieldID f = nullptr;
    JField(f, this->GetClass(), FLD_Inventory_selected, "I");
    if (!f) { lc->env->ExceptionClear(); return 0; }
    jint v = lc->env->GetIntField(this->instance, f);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return 0; }
    return (int)v;
}
