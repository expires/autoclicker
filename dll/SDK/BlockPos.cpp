#include "BlockPos.h"
#include "Mappings.h"

jclass BlockPos::GetClass() { return lc->GetClass(MC_BlockPos); }

BlockPos BlockPos::make(int x, int y, int z)
{
    jclass cls = GetClass();
    if (!cls) { lc->env->ExceptionClear(); return BlockPos(nullptr); }

    jmethodID ctor = lc->env->GetMethodID(cls, "<init>", "(III)V");
    if (!ctor) { lc->env->ExceptionClear(); return BlockPos(nullptr); }

    jobject obj = lc->env->NewObject(cls, ctor, (jint)x, (jint)y, (jint)z);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return BlockPos(nullptr); }
    return BlockPos(obj);
}
