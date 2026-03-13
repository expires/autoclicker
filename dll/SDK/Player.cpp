#include "Player.h"
#include "Mappings.h"

jclass Player::GetClass()
{
	return lc->GetClass(MC_LocalPlayer);
}
