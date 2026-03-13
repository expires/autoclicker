#include "MultiPlayerGameMode.h"

MultiPlayerGameMode::MultiPlayerGameMode(jobject instance)
{
	this->instance = instance;
}

jclass MultiPlayerGameMode::GetClass()
{
	return lc->GetClass("net.minecraft.class_636");
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
	jmethodID isDestroying = lc->env->GetMethodID(this->GetClass(), "method_2923", "()Z");

	bool rtn = lc->env->CallBooleanMethod(this->GetInstance(), isDestroying);

	return rtn;
}

int MultiPlayerGameMode::getDestroyStage()
{
	jmethodID destroyStage = lc->env->GetMethodID(this->GetClass(), "method_51888", "()I");

	int rtn = lc->env->CallBooleanMethod(this->GetInstance(), destroyStage);

	return rtn;
}

int MultiPlayerGameMode::getPlayerMode()
{
	jmethodID getPlayerMode = lc->env->GetMethodID(this->GetClass(), "method_2920", "()Lnet/minecraft/class_1934;");
	jobject typeObj = lc->env->CallObjectMethod(this->GetInstance(), getPlayerMode);

	jclass typeClass = lc->env->GetObjectClass(typeObj);
	jmethodID ordinalMethod = lc->env->GetMethodID(typeClass, "ordinal", "()I");

	jint rtn = lc->env->CallIntMethod(typeObj, ordinalMethod);

	return rtn;
}