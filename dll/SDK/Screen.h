#pragma once
#include "Lunar.h"

class Screen
{
public:
	Screen(jobject instance);

	jclass GetClass();

	void Cleanup();

	jobject GetInstance();

	bool isPauseScreen();

	bool shouldCloseOnEsc();

private:
	jobject screenInstance;
};
