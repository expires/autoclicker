#include "Player.h"
#include "Mappings.h"

jclass Player::GetClass()
{
	static jclass c = nullptr;
	return JClass(c, MC_LocalPlayer);
}
