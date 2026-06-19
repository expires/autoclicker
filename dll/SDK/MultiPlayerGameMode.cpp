#include "MultiPlayerGameMode.h"
#include "Mappings.h"

MultiPlayerGameMode::MultiPlayerGameMode(jobject instance)
{
	this->instance = instance;
}

jclass MultiPlayerGameMode::GetClass()
{
	static jclass c = nullptr;
	return JClass(c, MC_MultiPlayerGameMode);
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
	static jmethodID isDestroying = nullptr;
	JMethod(isDestroying, this->GetClass(), MTD_MPGM_isDestroying, "()Z");
	return lc->env->CallBooleanMethod(this->GetInstance(), isDestroying);
}

int MultiPlayerGameMode::getDestroyStage()
{
	static jmethodID destroyStage = nullptr;
	JMethod(destroyStage, this->GetClass(), MTD_MPGM_getDestroyStage, "()I");
	return lc->env->CallIntMethod(this->GetInstance(), destroyStage);
}

int MultiPlayerGameMode::getPlayerMode()
{
	static jmethodID getPlayerMode = nullptr;
	JMethod(getPlayerMode, this->GetClass(), MTD_MPGM_getPlayerMode, DESC_MPGM_getPlayerMode);
	jobject typeObj = lc->env->CallObjectMethod(this->GetInstance(), getPlayerMode);

	static jclass typeClass = nullptr;
	static jmethodID ordinalMethod = nullptr;
	if (!typeClass) JClass(typeClass, MC_GameType);
	JMethod(ordinalMethod, typeClass, "ordinal", "()I");
	return lc->env->CallIntMethod(typeObj, ordinalMethod);
}
