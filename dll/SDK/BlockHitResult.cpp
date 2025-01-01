#include "BlockHitResult.h"

BlockHitResult::BlockHitResult(jobject instance)
{
    this->blockHitResultInstance = instance;
}

jclass BlockHitResult::GetClass()
{
    return lc->GetClass("net.minecraft.world.phys.BlockHitResult");
}

void BlockHitResult::Cleanup()
{
    lc->env->DeleteLocalRef(this->blockHitResultInstance);
}

jobject BlockHitResult::GetInstance()
{
    return this->blockHitResultInstance;
}

BlockPos BlockHitResult::getBlockPos()
{
    jmethodID getBlockPos = lc->env->GetMethodID(this->GetClass(), "getBlockPos", "()Lnet/minecraft/core/BlockPos;");
    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), getBlockPos);

    return BlockPos(rtn);
}