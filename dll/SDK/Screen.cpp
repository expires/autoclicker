#include "Screen.h"
#include "Mappings.h"

Screen::Screen(jobject instance)
{
	this->instance = instance;
}

jclass Screen::GetClass()
{
	static jclass c = nullptr;
	return JClass(c, MC_Screen);
}

void Screen::Cleanup()
{
	lc->env->DeleteLocalRef(this->instance);
}

jobject Screen::GetInstance()
{
	return this->instance;
}

bool Screen::isPauseScreen()
{
	static jmethodID isPauseScreen = nullptr;
	JMethod(isPauseScreen, this->GetClass(), MTD_Screen_isPauseScreen, "()Z");
	return lc->env->CallBooleanMethod(this->GetInstance(), isPauseScreen);
}

bool Screen::shouldCloseOnEsc()
{
	static jmethodID shouldCloseOnEsc = nullptr;
	JMethod(shouldCloseOnEsc, this->GetClass(), MTD_Screen_shouldCloseOnEsc, "()Z");
	return lc->env->CallBooleanMethod(this->GetInstance(), shouldCloseOnEsc);
}
