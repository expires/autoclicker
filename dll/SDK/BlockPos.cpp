#include "BlockPos.h"

BlockPos::BlockPos(jobject instance)
{
    this->blockPosInstance = instance;
}

jclass BlockPos::GetClass()
{
    return lc->GetClass("net.minecraft.core.BlockPos");
}

void BlockPos::Cleanup()
{
    lc->env->DeleteLocalRef(this->blockPosInstance);
}

jobject BlockPos::GetInstance()
{
    return this->blockPosInstance;
}