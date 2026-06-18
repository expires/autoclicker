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
    if (this->instance == nullptr) return Item(nullptr);
    jmethodID getItem = lc->env->GetMethodID(this->GetClass(),
        MTD_ItemStack_getItem, DESC_ItemStack_getItem);
    if (!getItem) { lc->env->ExceptionClear(); return Item(nullptr); }
    jobject rtn = lc->env->CallObjectMethod(this->instance, getItem);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return Item(nullptr); }
    return Item(rtn);
}

Component ItemStack::getHoverName()
{
    if (this->instance == nullptr) return Component(nullptr);
    jmethodID m = lc->env->GetMethodID(this->GetClass(),
        MTD_ItemStack_getHoverName, DESC_ItemStack_getHoverName);
    if (!m) { lc->env->ExceptionClear(); return Component(nullptr); }
    jobject rtn = lc->env->CallObjectMethod(this->instance, m);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return Component(nullptr); }
    return Component(rtn);
}

bool ItemStack::isEmpty()
{
    if (this->instance == nullptr) return true;
    jmethodID m = lc->env->GetMethodID(this->GetClass(),
        MTD_ItemStack_isEmpty, "()Z");
    if (!m) { lc->env->ExceptionClear(); return true; }
    jboolean v = lc->env->CallBooleanMethod(this->instance, m);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return true; }
    return v == JNI_TRUE;
}
