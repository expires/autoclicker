#pragma once
#include "Lunar.h"
#include "ItemStack.h"

class Player
{
public:
	Player(jobject instance);

	jclass GetClass();

	void Cleanup();

	jobject GetInstance();

	ItemStack getItemInHand();

	bool isUsingItem();

private:
	jobject instance;
};
