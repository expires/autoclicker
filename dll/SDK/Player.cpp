#include "Player.h"
#include "Mappings.h"

jclass Player::GetClass()
{
	return lc->GetClass(MC_LocalPlayer);
}

Inventory Player::getInventory()
{
	jmethodID m = lc->env->GetMethodID(this->GetClass(),
		MTD_Player_getInventory, DESC_Player_getInventory);
	if (!m) { lc->env->ExceptionClear(); return Inventory(nullptr); }
	jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), m);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return Inventory(nullptr); }
	return Inventory(rtn);
}
