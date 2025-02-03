#pragma once
#include "Lunar.h"

class MultiPlayerGameMode
{
public:
	MultiPlayerGameMode(jobject instance);

	jclass GetClass();

	void Cleanup();

	jobject GetInstance();

	bool isDestroying();

	int getDestroyStage();

	int getPlayerMode();

private:
	jobject instance;
};
