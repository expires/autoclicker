#include "Screen.h"

Screen::Screen(jobject instance)
{
	this->instance = instance;
}

jclass Screen::GetClass()
{
	return lc->GetClass("net.minecraft.client.gui.screens.Screen");
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
	jmethodID isPauseScreen = lc->env->GetMethodID(this->GetClass(), "isPauseScreen", "()Z");
	bool rtn = lc->env->CallBooleanMethod(this->GetInstance(), isPauseScreen);

	return rtn;
}

bool Screen::shouldCloseOnEsc()
{
	jmethodID shouldCloseOnEsc = lc->env->GetMethodID(this->GetClass(), "shouldCloseOnEsc", "()Z");
	bool rtn = lc->env->CallBooleanMethod(this->GetInstance(), shouldCloseOnEsc);

	return rtn;
}