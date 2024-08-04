#pragma once
#include "Lunar.h"

class Screen
{
public:
	Screen(jobject instance);

	jclass GetClass();

	void Cleanup();

	bool isPauseScreen();

	bool shouldCloseOnEsc();

private:
	jobject screenInstance;
};

