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

jobject MultiPlayerGameMode::GetInstance()
{
	return this->mpgmInstance;
}

bool MultiPlayerGameMode::isDestroying()
{
	jmethodID isDestroying = lc->env->GetMethodID(this->GetClass(), "isDestroying", "()Z");

	bool rtn = lc->env->CallBooleanMethod(this->GetInstance(), isDestroying);

	return rtn;
}

int MultiPlayerGameMode::getDestroyStage()
{
	jmethodID destroyStage = lc->env->GetMethodID(this->GetClass(), "getDestroyStage", "()I");

	int rtn = lc->env->CallBooleanMethod(this->GetInstance(), destroyStage);

	return rtn;
}

int MultiPlayerGameMode::getPlayerMode()
{
	jmethodID getPlayerMode = lc->env->GetMethodID(this->GetClass(), "getPlayerMode", "()Lnet/minecraft/world/level/GameType;");
	jobject typeObj = lc->env->CallObjectMethod(this->GetInstance(), getPlayerMode);

	jclass typeClass = lc->env->GetObjectClass(typeObj);
	jmethodID ordinalMethod = lc->env->GetMethodID(typeClass, "ordinal", "()I");

	jint rtn = lc->env->CallIntMethod(typeObj, ordinalMethod);

	return rtn;
}