#include "BlockPos.h"
#include "Mappings.h"

jclass BlockPos::GetClass() { static jclass c = nullptr; return JClass(c, MC_BlockPos); }

BlockPos BlockPos::make(int x, int y, int z)
{
    jclass cls = GetClass();
    if (!cls) { lc->env->ExceptionClear(); return BlockPos(nullptr); }

    static jmethodID ctor = nullptr;
    JMethod(ctor, cls, "<init>", "(III)V");
    if (!ctor) { lc->env->ExceptionClear(); return BlockPos(nullptr); }

    jobject obj = lc->env->NewObject(cls, ctor, (jint)x, (jint)y, (jint)z);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return BlockPos(nullptr); }
    return BlockPos(obj);
}
