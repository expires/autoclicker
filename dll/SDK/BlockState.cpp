#include "BlockState.h"
#include "Mappings.h"

jclass BlockState::GetClass() { static jclass c = nullptr; return JClass(c, MC_BlockState); }

bool BlockState::isAir()
{
    if (!instance) return true;
    static jmethodID m = nullptr;
    JMethod(m, this->GetClass(), MTD_BlockState_isAir, "()Z");
    if (!m) { lc->env->ExceptionClear(); return true; }
    jboolean r = lc->env->CallBooleanMethod(this->instance, m);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return true; }
    return r == JNI_TRUE;
}
