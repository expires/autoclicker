#include "BlockState.h"
#include "Mappings.h"

bool BlockState::blocksMotion()
{
    if (!instance) return false;
    static jmethodID m = nullptr;
    JMethod(m, this->GetClass(), MTD_BlockState_blocksMotion, "()Z");
    if (!m) { lc->env->ExceptionClear(); return false; }
    jboolean r = lc->env->CallBooleanMethod(this->instance, m);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return false; }
    return r == JNI_TRUE;
}
