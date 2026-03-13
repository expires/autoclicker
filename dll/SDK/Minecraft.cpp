#include "Minecraft.h"

jclass Minecraft::GetClass()
{
	return lc->GetClass("net.minecraft.class_310");
}

jobject Minecraft::GetInstance()
{

	jfieldID getMinecraft = lc->env->GetStaticFieldID(this->GetClass(), "field_1700", "Lnet/minecraft/class_310;");
	jobject rtn = lc->env->GetStaticObjectField(this->GetClass(), getMinecraft);

	if (rtn == nullptr)
		return nullptr;

	return rtn;
}

Player Minecraft::GetLocalPlayer()
{

	jfieldID getPlayer = lc->env->GetFieldID(this->GetClass(), "field_1724", "Lnet/minecraft/class_746;");
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), getPlayer);

	if (rtn == nullptr)
		return nullptr;

	return Player(rtn);
}

Gui Minecraft::GetGui()
{
	jfieldID getGui = lc->env->GetFieldID(this->GetClass(), "field_1705", "Lnet/minecraft/class_329;");
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), getGui);

	if (rtn == nullptr)
		return nullptr;

	return Gui(rtn);
}

MultiPlayerGameMode Minecraft::GetMultiPlayerGameMode()
{
	jfieldID getMPGM = lc->env->GetFieldID(this->GetClass(), "field_1761", "Lnet/minecraft/class_636;");
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), getMPGM);

	if (rtn == nullptr)
		return nullptr;

	return MultiPlayerGameMode(rtn);
}

Screen Minecraft::GetScreen()
{
	jfieldID getScreen = lc->env->GetFieldID(this->GetClass(), "field_1755", "Lnet/minecraft/class_437;");
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), getScreen);

	if (rtn == nullptr)
		return nullptr;

	return Screen(rtn);
}

bool Minecraft::isPaused()
{
	jmethodID isPaused = lc->env->GetMethodID(this->GetClass(), "method_1493", "()Z");
	bool rtn = lc->env->CallBooleanMethod(this->GetInstance(), isPaused);

	return rtn;
}

HitResult Minecraft::getHitResult()
{
	jfieldID getHitResult = lc->env->GetFieldID(this->GetClass(), "field_1765", "Lnet/minecraft/class_239;");
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), getHitResult);

	if (rtn == nullptr)
		return nullptr;

	return HitResult(rtn);
}