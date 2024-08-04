#include "Player.h"

Player::Player(jobject instance)
{
	this->playerInstance = instance;
}

jclass Player::GetClass()
{
	return lc->GetClass("net.minecraft.client.player.LocalPlayer");
}

void Player::Cleanup()
{
	lc->env->DeleteLocalRef(this->playerInstance);
}