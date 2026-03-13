#include "Screen.h"
#include "Mappings.h"

Screen::Screen(jobject instance)
{
	this->instance = instance;
}

jclass Screen::GetClass()
{
	return lc->GetClass(MC_Screen);
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
	jmethodID isPauseScreen = lc->env->GetMethodID(this->GetClass(), MTD_Screen_isPauseScreen, "()Z");
	return lc->env->CallBooleanMethod(this->GetInstance(), isPauseScreen);
}

bool Screen::shouldCloseOnEsc()
{
	jmethodID shouldCloseOnEsc = lc->env->GetMethodID(this->GetClass(), MTD_Screen_shouldCloseOnEsc, "()Z");
	return lc->env->CallBooleanMethod(this->GetInstance(), shouldCloseOnEsc);
}
