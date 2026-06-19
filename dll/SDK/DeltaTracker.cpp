#include "DeltaTracker.h"
#include "Mappings.h"

jclass DeltaTracker::GetClass() { static jclass c = nullptr; return JClass(c, MC_DeltaTracker); }

float DeltaTracker::getPartialTick(bool runsNormally)
{
    static jmethodID m = nullptr;
    JMethod(m, this->GetClass(), MTD_DeltaTracker_getPartialTick, DESC_DeltaTracker_getPartialTick);
    if (!m) { lc->env->ExceptionClear(); return 0.0f; }
    float v = lc->env->CallFloatMethod(this->instance, m, (jboolean)runsNormally);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return 0.0f; }
    return v;
}
