#include "Minecraft.h"
#include "Mappings.h"

jclass Minecraft::GetClass()
{
	static jclass c = nullptr;
	return JClass(c, MC_Minecraft);
}

jobject Minecraft::GetInstance()
{
	jclass cls = this->GetClass();
	if (cls == nullptr) return nullptr;
	static jfieldID getMinecraft = nullptr;
	JStaticField(getMinecraft, cls, FLD_Minecraft_instance, DESC_Minecraft_instance);
	if (getMinecraft == nullptr) return nullptr;
	jobject rtn = lc->env->GetStaticObjectField(cls, getMinecraft);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return nullptr; }
	return rtn;
}

Player Minecraft::GetLocalPlayer()
{
	jobject inst = this->GetInstance();
	if (inst == nullptr) return nullptr;
	static jfieldID getPlayer = nullptr;
	JField(getPlayer, this->GetClass(), FLD_Minecraft_player, DESC_Minecraft_player);
	if (getPlayer == nullptr) return nullptr;
	jobject rtn = lc->env->GetObjectField(inst, getPlayer);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return nullptr; }
	return Player(rtn);
}

Gui Minecraft::GetGui()
{
	jobject inst = this->GetInstance();
	if (inst == nullptr) return nullptr;
	static jfieldID getGui = nullptr;
	JField(getGui, this->GetClass(), FLD_Minecraft_gui, DESC_Minecraft_gui);
	if (getGui == nullptr) return nullptr;
	jobject rtn = lc->env->GetObjectField(inst, getGui);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return nullptr; }
	return Gui(rtn);
}

MultiPlayerGameMode Minecraft::GetMultiPlayerGameMode()
{
	jobject inst = this->GetInstance();
	if (inst == nullptr) return nullptr;
	static jfieldID getMPGM = nullptr;
	JField(getMPGM, this->GetClass(), FLD_Minecraft_gameMode, DESC_Minecraft_gameMode);
	if (getMPGM == nullptr) return nullptr;
	jobject rtn = lc->env->GetObjectField(inst, getMPGM);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return nullptr; }
	return MultiPlayerGameMode(rtn);
}

Screen Minecraft::GetScreen()
{
	jobject inst = this->GetInstance();
	if (inst == nullptr) return nullptr;
	static jfieldID getScreen = nullptr;
	JField(getScreen, this->GetClass(), FLD_Minecraft_screen, DESC_Minecraft_screen);
	if (getScreen == nullptr) return nullptr;
	jobject rtn = lc->env->GetObjectField(inst, getScreen);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return nullptr; }
	return Screen(rtn);
}

bool Minecraft::isPaused()
{
	jobject inst = this->GetInstance();
	if (inst == nullptr) return false;
	static jmethodID isPaused = nullptr;
	JMethod(isPaused, this->GetClass(), MTD_Minecraft_isPaused, "()Z");
	if (isPaused == nullptr) { lc->env->ExceptionClear(); return false; }
	jboolean v = lc->env->CallBooleanMethod(inst, isPaused);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return false; }
	return v == JNI_TRUE;
}

HitResult Minecraft::getHitResult()
{
	jobject inst = this->GetInstance();
	if (inst == nullptr) return nullptr;
	static jfieldID getHitResult = nullptr;
	JField(getHitResult, this->GetClass(), FLD_Minecraft_hitResult, DESC_Minecraft_hitResult);
	if (getHitResult == nullptr) return nullptr;
	jobject rtn = lc->env->GetObjectField(inst, getHitResult);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return nullptr; }
	return HitResult(rtn);
}

Level Minecraft::GetLevel()
{
	jobject inst = this->GetInstance();
	if (inst == nullptr) return Level(nullptr);
	static jfieldID f = nullptr;
	JField(f, this->GetClass(), FLD_Minecraft_level, DESC_Minecraft_level);
	if (f == nullptr) return Level(nullptr);
	jobject rtn = lc->env->GetObjectField(inst, f);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return Level(nullptr); }
	return Level(rtn);
}

GameRenderer Minecraft::GetGameRenderer()
{
	jobject inst = this->GetInstance();
	if (inst == nullptr) return GameRenderer(nullptr);
	static jfieldID f = nullptr;
	JField(f, this->GetClass(), FLD_Minecraft_gameRenderer, DESC_Minecraft_gameRenderer);
	if (f == nullptr) return GameRenderer(nullptr);
	jobject rtn = lc->env->GetObjectField(inst, f);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return GameRenderer(nullptr); }
	return GameRenderer(rtn);
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
