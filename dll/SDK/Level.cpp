#include "Level.h"
#include "Mappings.h"

jclass Level::GetClass() { static jclass c = nullptr; return JClass(c, MC_ClientLevel); }

BlockState Level::getBlockState(BlockPos& pos)
{
    if (!instance || !pos.GetInstance()) return BlockState(nullptr);
    static jmethodID m = nullptr;
    JMethod(m, this->GetClass(), MTD_Level_getBlockState, DESC_Level_getBlockState);
    if (!m) { lc->env->ExceptionClear(); return BlockState(nullptr); }
    jobject r = lc->env->CallObjectMethod(this->instance, m, pos.GetInstance());
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return BlockState(nullptr); }
    return BlockState(r);
}
