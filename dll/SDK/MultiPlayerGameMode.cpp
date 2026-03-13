#include "MultiPlayerGameMode.h"
#include "Mappings.h"

MultiPlayerGameMode::MultiPlayerGameMode(jobject instance)
{
	this->instance = instance;
}

jclass MultiPlayerGameMode::GetClass()
{
	return lc->GetClass(MC_MultiPlayerGameMode);
}

void MultiPlayerGameMode::Cleanup()
{
	lc->env->DeleteLocalRef(this->instance);
}

jobject MultiPlayerGameMode::GetInstance()
{
	return this->instance;
}

bool MultiPlayerGameMode::isDestroying()
{
	jmethodID isDestroying = lc->env->GetMethodID(this->GetClass(), MTD_MPGM_isDestroying, "()Z");
	return lc->env->CallBooleanMethod(this->GetInstance(), isDestroying);
}

int MultiPlayerGameMode::getDestroyStage()
{
	jmethodID destroyStage = lc->env->GetMethodID(this->GetClass(), MTD_MPGM_getDestroyStage, "()I");
	return lc->env->CallIntMethod(this->GetInstance(), destroyStage);
}

int MultiPlayerGameMode::getPlayerMode()
{
	jmethodID getPlayerMode = lc->env->GetMethodID(this->GetClass(), MTD_MPGM_getPlayerMode, DESC_MPGM_getPlayerMode);
	jobject typeObj = lc->env->CallObjectMethod(this->GetInstance(), getPlayerMode);

	jclass typeClass = lc->env->GetObjectClass(typeObj);
	jmethodID ordinalMethod = lc->env->GetMethodID(typeClass, "ordinal", "()I");
	return lc->env->CallIntMethod(typeObj, ordinalMethod);
}
