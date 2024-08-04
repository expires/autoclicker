#include "Minecraft.h"

jclass Minecraft::GetClass()
{
	return lc->GetClass("net.minecraft.client.Minecraft");
}

jobject Minecraft::GetInstance()
{

	jclass minecraftClass = this->GetClass();

	jfieldID getMinecraft = lc->env->GetStaticFieldID(minecraftClass, "instance", "Lnet/minecraft/client/Minecraft;");
	jobject rtn = lc->env->GetStaticObjectField(minecraftClass, getMinecraft);

	if (rtn == nullptr) return nullptr;

	return rtn;
}

Player Minecraft::GetLocalPlayer()
{

	jclass minecraftClass = this->GetClass();
	jobject minecraftObject = this->GetInstance();

	jfieldID getPlayer = lc->env->GetFieldID(minecraftClass, "player", "Lnet/minecraft/client/player/LocalPlayer;");
	jobject rtn = lc->env->GetObjectField(minecraftObject, getPlayer);

	if (rtn == nullptr) return nullptr;

	return Player(rtn);
}

MultiPlayerGameMode Minecraft::GetMultiPlayerGameMode()
{
	jclass minecraftClass = this->GetClass();
	jobject minecraftObject = this->GetInstance();

	jfieldID getMPGM = lc->env->GetFieldID(minecraftClass, "gameMode", "Lnet/minecraft/client/multiplayer/MultiPlayerGameMode;");
	jobject rtn = lc->env->GetObjectField(minecraftObject, getMPGM);

	if (rtn == nullptr) return nullptr;

	return MultiPlayerGameMode(rtn);
}

Screen Minecraft::GetScreen()
{
	jclass minecraftClass = this->GetClass();
	jobject minecraftObject = this->GetInstance();

	jfieldID getScreen = lc->env->GetFieldID(minecraftClass, "screen", "Lnet/minecraft/client/gui/screens/Screen;");
	jobject rtn = lc->env->GetObjectField(minecraftObject, getScreen);

	if (rtn == nullptr) return nullptr;

	return Screen(rtn);
}

bool Minecraft::isPaused()
{
	jclass minecraftClass = this->GetClass();
	jobject minecraftObject = this->GetInstance();

	jmethodID isPaused = lc->env->GetMethodID(minecraftClass, "isPaused", "()Z");
	bool rtn = lc->env->CallBooleanMethod(minecraftObject, isPaused);

	return rtn;
}
