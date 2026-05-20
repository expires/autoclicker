#include "BlockState.h"
#include "Mappings.h"

jclass BlockState::GetClass() { return lc->GetClass(MC_BlockState); }

bool BlockState::isAir()
{
    if (!instance) return true;
    jmethodID m = lc->env->GetMethodID(this->GetClass(), MTD_BlockState_isAir, "()Z");
    if (!m) { lc->env->ExceptionClear(); return true; }
    jboolean r = lc->env->CallBooleanMethod(this->instance, m);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return true; }
    return r == JNI_TRUE;
}
