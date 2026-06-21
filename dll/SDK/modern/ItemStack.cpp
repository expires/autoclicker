#include "ItemStack.h"
#include "Mappings.h"

bool ItemStack::isEmpty()
{
    if (this->instance == nullptr) return true;
    static jmethodID m = nullptr;
    JMethod(m, this->GetClass(), MTD_ItemStack_isEmpty, "()Z");
    if (!m) { lc->env->ExceptionClear(); return true; }
    jboolean v = lc->env->CallBooleanMethod(this->instance, m);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return true; }
    return v == JNI_TRUE;
}
