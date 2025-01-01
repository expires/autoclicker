#include "Minecraft.h"

jclass Minecraft::GetClass()
{
	return lc->GetClass("net.minecraft.client.Minecraft");
}

jobject Minecraft::GetInstance()
{

	jfieldID getMinecraft = lc->env->GetStaticFieldID(this->GetClass(), "instance", "Lnet/minecraft/client/Minecraft;");
	jobject rtn = lc->env->GetStaticObjectField(this->GetClass(), getMinecraft);

	if (rtn == nullptr)
		return nullptr;

	return rtn;
}

Player Minecraft::GetLocalPlayer()
{

	jfieldID getPlayer = lc->env->GetFieldID(this->GetClass(), "player", "Lnet/minecraft/client/player/LocalPlayer;");
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), getPlayer);

	if (rtn == nullptr)
		return nullptr;

	return Player(rtn);
}

MultiPlayerGameMode Minecraft::GetMultiPlayerGameMode()
{
	jfieldID getMPGM = lc->env->GetFieldID(this->GetClass(), "gameMode", "Lnet/minecraft/client/multiplayer/MultiPlayerGameMode;");
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), getMPGM);

	if (rtn == nullptr)
		return nullptr;

	return MultiPlayerGameMode(rtn);
}

Screen Minecraft::GetScreen()
{
	jfieldID getScreen = lc->env->GetFieldID(this->GetClass(), "screen", "Lnet/minecraft/client/gui/screens/Screen;");
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), getScreen);

	if (rtn == nullptr)
		return nullptr;

	return Screen(rtn);
}

bool Minecraft::isPaused()
{
	jmethodID isPaused = lc->env->GetMethodID(this->GetClass(), "isPaused", "()Z");
	bool rtn = lc->env->CallBooleanMethod(this->GetInstance(), isPaused);

	return rtn;
}

HitResult Minecraft::getHitResult()
{
	jfieldID getHitResult = lc->env->GetFieldID(this->GetClass(), "hitResult", "Lnet/minecraft/world/phys/HitResult;");
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), getHitResult);

	if (rtn == nullptr)
		return nullptr;

	return HitResult(rtn);
}
