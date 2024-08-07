#pragma once
#include "Lunar.h"

class MultiPlayerGameMode
{
public:
	MultiPlayerGameMode(jobject instance);

	jclass GetClass();

	void Cleanup();

	bool isDestroying();

	int getDestroyStage();

private:
	jobject mpgmInstance;
};

