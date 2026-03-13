#include "Screen.h"

Screen::Screen(jobject instance)
{
	this->instance = instance;
}

jclass Screen::GetClass()
{
	return lc->GetClass("net.minecraft.class_437");
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
	jmethodID isPauseScreen = lc->env->GetMethodID(this->GetClass(), "method_25421", "()Z");
	bool rtn = lc->env->CallBooleanMethod(this->GetInstance(), isPauseScreen);

	return rtn;
}

bool Screen::shouldCloseOnEsc()
{
	jmethodID shouldCloseOnEsc = lc->env->GetMethodID(this->GetClass(), "method_25422", "()Z");
	bool rtn = lc->env->CallBooleanMethod(this->GetInstance(), shouldCloseOnEsc);

	return rtn;
}