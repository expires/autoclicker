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

