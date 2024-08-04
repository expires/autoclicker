#pragma once
#include "Lunar.h"
#include "Player.h"
#include "MultiPlayerGameMode.h"
#include "Screen.h"


class Minecraft
{
public:
	jclass GetClass();

	jobject GetInstance();

	Player GetLocalPlayer();

	MultiPlayerGameMode GetMultiPlayerGameMode();

	Screen GetScreen();

	bool isPaused();

	bool isWindowActive();
};

