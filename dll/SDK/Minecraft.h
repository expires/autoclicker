#pragma once
#include "Lunar.h"
#include "Player.h"
#include "MultiPlayerGameMode.h"
#include "Screen.h"
#include "HitResult.h"
#include "Gui.h"
#include "Level.h"

class Minecraft
{
public:
	jclass GetClass();

	jobject GetInstance();

	Player GetLocalPlayer();

	MultiPlayerGameMode GetMultiPlayerGameMode();

	Screen GetScreen();

	Gui GetGui();

	bool isPaused();

	HitResult getHitResult();

	Level GetLevel();

};
