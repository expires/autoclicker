#include "DeltaTracker.h"
#include "Mappings.h"

jclass DeltaTracker::GetClass() { return lc->GetClass(MC_DeltaTracker); }

float DeltaTracker::getPartialTick(bool runsNormally)
{
    jmethodID m = lc->env->GetMethodID(this->GetClass(),
        MTD_DeltaTracker_getPartialTick, DESC_DeltaTracker_getPartialTick);
    if (!m) { lc->env->ExceptionClear(); return 0.0f; }
    float v = lc->env->CallFloatMethod(this->instance, m, (jboolean)runsNormally);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return 0.0f; }
    return v;
}
