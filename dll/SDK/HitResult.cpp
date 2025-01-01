#include "HitResult.h"

HitResult::HitResult(jobject instance)
{
    this->hitResultInstance = instance;
}

jclass HitResult::GetClass()
{
    return lc->GetClass("net.minecraft.world.phys.HitResult");
}

void HitResult::Cleanup()
{
    lc->env->DeleteLocalRef(this->hitResultInstance);
}

jobject HitResult::GetInstance()
{
    return this->hitResultInstance;
}

int HitResult::getType()
{
    jmethodID hitType = lc->env->GetMethodID(this->GetClass(), "getType", "()Lnet/minecraft/world/phys/HitResult$Type;");
    jobject typeObj = lc->env->CallObjectMethod(this->GetInstance(), hitType);

    jclass typeClass = lc->env->GetObjectClass(typeObj);
    jmethodID ordinalMethod = lc->env->GetMethodID(typeClass, "ordinal", "()I");
    jint rtn = lc->env->CallIntMethod(typeObj, ordinalMethod);

    return rtn;
}

BlockHitResult HitResult::getBlockHitResult()
{
    if (this->getType() != 1)
        return nullptr;

    jclass blockHitResultClass = lc->env->FindClass("net/minecraft/world/phys/BlockHitResult");
    if (lc->env->IsInstanceOf(this->hitResultInstance, blockHitResultClass))
    {
        return BlockHitResult(this->hitResultInstance);
    }
}