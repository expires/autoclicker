#include "Minecraft.h"
#include "Mappings.h"

jclass Minecraft::GetClass()
{
	return lc->GetClass(MC_Minecraft);
}

jobject Minecraft::GetInstance()
{
	jfieldID getMinecraft = lc->env->GetStaticFieldID(this->GetClass(), FLD_Minecraft_instance, DESC_Minecraft_instance);
	jobject rtn = lc->env->GetStaticObjectField(this->GetClass(), getMinecraft);

	if (rtn == nullptr)
		return nullptr;

	return rtn;
}

Player Minecraft::GetLocalPlayer()
{
	jfieldID getPlayer = lc->env->GetFieldID(this->GetClass(), FLD_Minecraft_player, DESC_Minecraft_player);
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), getPlayer);

	if (rtn == nullptr)
		return nullptr;

	return Player(rtn);
}

Gui Minecraft::GetGui()
{
	jfieldID getGui = lc->env->GetFieldID(this->GetClass(), FLD_Minecraft_gui, DESC_Minecraft_gui);
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), getGui);

	if (rtn == nullptr)
		return nullptr;

	return Gui(rtn);
}

MultiPlayerGameMode Minecraft::GetMultiPlayerGameMode()
{
	jfieldID getMPGM = lc->env->GetFieldID(this->GetClass(), FLD_Minecraft_gameMode, DESC_Minecraft_gameMode);
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), getMPGM);

	if (rtn == nullptr)
		return nullptr;

	return MultiPlayerGameMode(rtn);
}

Screen Minecraft::GetScreen()
{
	jfieldID getScreen = lc->env->GetFieldID(this->GetClass(), FLD_Minecraft_screen, DESC_Minecraft_screen);
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), getScreen);

	if (rtn == nullptr)
		return nullptr;

	return Screen(rtn);
}

bool Minecraft::isPaused()
{
	jmethodID isPaused = lc->env->GetMethodID(this->GetClass(), MTD_Minecraft_isPaused, "()Z");
	return lc->env->CallBooleanMethod(this->GetInstance(), isPaused);
}

HitResult Minecraft::getHitResult()
{
	jfieldID getHitResult = lc->env->GetFieldID(this->GetClass(), FLD_Minecraft_hitResult, DESC_Minecraft_hitResult);
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), getHitResult);

	if (rtn == nullptr)
		return nullptr;

	return HitResult(rtn);
}

Level Minecraft::GetLevel()
{
	jfieldID f = lc->env->GetFieldID(this->GetClass(), FLD_Minecraft_level, DESC_Minecraft_level);
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), f);

	if (rtn == nullptr)
		return nullptr;

	return Level(rtn);
}
