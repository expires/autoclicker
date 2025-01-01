#pragma once
#include "Lunar.h"

class Player
{
public:
	Player(jobject instance);

	jclass GetClass();

	void Cleanup();

private:
	jobject playerInstance;
};
