#include "Player.h"

jclass Player::GetClass()
{
	return lc->GetClass("net.minecraft.client.player.LocalPlayer");
}