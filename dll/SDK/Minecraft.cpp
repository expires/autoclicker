#include "Minecraft.h"
#include "Mappings.h"

jclass Minecraft::GetClass()
{
	static jclass c = nullptr;
	return JClass(c, MC_Minecraft);
}

jobject Minecraft::GetInstance()
{
	static jfieldID getMinecraft = nullptr;
	JStaticField(getMinecraft, this->GetClass(), FLD_Minecraft_instance, DESC_Minecraft_instance);
	jobject rtn = lc->env->GetStaticObjectField(this->GetClass(), getMinecraft);

	if (rtn == nullptr)
		return nullptr;

	return rtn;
}

Player Minecraft::GetLocalPlayer()
{
	static jfieldID getPlayer = nullptr;
	JField(getPlayer, this->GetClass(), FLD_Minecraft_player, DESC_Minecraft_player);
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), getPlayer);

	if (rtn == nullptr)
		return nullptr;

	return Player(rtn);
}

Gui Minecraft::GetGui()
{
	static jfieldID getGui = nullptr;
	JField(getGui, this->GetClass(), FLD_Minecraft_gui, DESC_Minecraft_gui);
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), getGui);

	if (rtn == nullptr)
		return nullptr;

	return Gui(rtn);
}

MultiPlayerGameMode Minecraft::GetMultiPlayerGameMode()
{
	static jfieldID getMPGM = nullptr;
	JField(getMPGM, this->GetClass(), FLD_Minecraft_gameMode, DESC_Minecraft_gameMode);
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), getMPGM);

	if (rtn == nullptr)
		return nullptr;

	return MultiPlayerGameMode(rtn);
}

Screen Minecraft::GetScreen()
{
	static jfieldID getScreen = nullptr;
	JField(getScreen, this->GetClass(), FLD_Minecraft_screen, DESC_Minecraft_screen);
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), getScreen);

	if (rtn == nullptr)
		return nullptr;

	return Screen(rtn);
}

bool Minecraft::isPaused()
{
	static jmethodID isPaused = nullptr;
	JMethod(isPaused, this->GetClass(), MTD_Minecraft_isPaused, "()Z");
	return lc->env->CallBooleanMethod(this->GetInstance(), isPaused);
}

HitResult Minecraft::getHitResult()
{
	static jfieldID getHitResult = nullptr;
	JField(getHitResult, this->GetClass(), FLD_Minecraft_hitResult, DESC_Minecraft_hitResult);
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), getHitResult);

	if (rtn == nullptr)
		return nullptr;

	return HitResult(rtn);
}

Level Minecraft::GetLevel()
{
	static jfieldID f = nullptr;
	JField(f, this->GetClass(), FLD_Minecraft_level, DESC_Minecraft_level);
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), f);
	return Level(rtn);
}

GameRenderer Minecraft::GetGameRenderer()
{
	static jfieldID f = nullptr;
	JField(f, this->GetClass(), FLD_Minecraft_gameRenderer, DESC_Minecraft_gameRenderer);
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), f);
	return GameRenderer(rtn);
}

DeltaTracker Minecraft::GetDeltaTracker()
{
	static jmethodID m = nullptr;
	JMethod(m, this->GetClass(), MTD_Minecraft_getDeltaTracker, DESC_Minecraft_getDeltaTracker);
	if (!m) { lc->env->ExceptionClear(); return DeltaTracker(nullptr); }
	jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), m);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return DeltaTracker(nullptr); }
	return DeltaTracker(rtn);
}

void Minecraft::setScreen(jobject screen)
{
	static jmethodID m = nullptr;
	JMethod(m, this->GetClass(), MTD_Minecraft_setScreen, DESC_Minecraft_setScreen);
	if (!m) { lc->env->ExceptionClear(); return; }
	lc->env->CallVoidMethod(this->GetInstance(), m, screen);
	if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
}

jobject Minecraft::newPauseScreen(bool showMenu)
{
	static jclass cls = nullptr;
	JClass(cls, MC_PauseScreen);
	if (!cls) return nullptr;
	static jmethodID ctor = nullptr;
	JMethod(ctor, cls, "<init>", "(Z)V");
	if (!ctor) { lc->env->ExceptionClear(); return nullptr; }
	jobject obj = lc->env->NewObject(cls, ctor, (jboolean)showMenu);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return nullptr; }
	return obj;
}

void Minecraft::pauseGame(bool pauseOnly)
{
	static jmethodID m = nullptr;
	JMethod(m, this->GetClass(), MTD_Minecraft_pauseGame, DESC_Minecraft_pauseGame);
	if (!m) { lc->env->ExceptionClear(); return; }
	lc->env->CallVoidMethod(this->GetInstance(), m, (jboolean)pauseOnly);
	if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
}
