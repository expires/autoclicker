#include "MultiPlayerGameMode.h"

MultiPlayerGameMode::MultiPlayerGameMode(jobject instance)
{
	this->mpgmInstance = instance;
}


jclass MultiPlayerGameMode::GetClass()
{
	return lc->GetClass("net.minecraft.client.multiplayer.MultiPlayerGameMode");
}

void MultiPlayerGameMode::Cleanup()
{
	lc->env->DeleteLocalRef(this->mpgmInstance);
}

bool MultiPlayerGameMode::isDestroying()
{
	jmethodID isDestroying = lc->env->GetMethodID(this->GetClass(), "isDestroying", "()Z");

	bool rtn = lc->env->CallBooleanMethod(this->mpgmInstance, isDestroying);

	return rtn;
}

int MultiPlayerGameMode::getDestroyStage()
{
	jmethodID destroyStage = lc->env->GetMethodID(this->GetClass(), "getDestroyStage", "()I");

	int rtn = lc->env->CallBooleanMethod(this->mpgmInstance, destroyStage);

	return rtn;
}