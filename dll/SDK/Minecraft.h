#pragma once
#include "Lunar.h"
#include "Player.h"
#include "MultiPlayerGameMode.h"
#include "Screen.h"
#include "HitResult.h"
#include "Gui.h"
#include "Level.h"
#include "GameRenderer.h"
#include "DeltaTracker.h"

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

	GameRenderer GetGameRenderer();

	DeltaTracker GetDeltaTracker();

	// Call Minecraft.setScreen(screen). Passing a null jobject closes the
	// current screen. Used by the overlay to pause input via MC's own
	// screen mechanism (LocalPlayer.aiStep skips movement when
	// Minecraft.screen != null) instead of swallowing WndProc messages.
	void setScreen(jobject screen);

	// Construct a new net.minecraft.client.gui.screens.PauseScreen(showMenu).
	// Returns a local ref; promote to global if you need to retain it across
	// JNI frames. Returns nullptr if anything in the construction chain fails.
	jobject newPauseScreen(bool showMenu);

	// Call Minecraft.pauseGame(boolean pauseOnly). MC's own pause API —
	// constructs PauseScreen inside Java so class loading goes through MC's
	// classloader (avoiding the "class not in our JVMTI snapshot" problem
	// we hit when constructing via NewObject ourselves). `pauseOnly=true`
	// opens a blank pause screen with no menu UI behind it.
	void pauseGame(bool pauseOnly);
};
