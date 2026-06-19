#include "Player.h"
#include "Mappings.h"

jclass Player::GetClass()
{
	static jclass c = nullptr;
	return JClass(c, MC_LocalPlayer);
}

Inventory Player::getInventory()
{
	static jmethodID m = nullptr;
	JMethod(m, this->GetClass(), MTD_Player_getInventory, DESC_Player_getInventory);
	if (!m) { lc->env->ExceptionClear(); return Inventory(nullptr); }
	jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), m);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return Inventory(nullptr); }
	return Inventory(rtn);
}
