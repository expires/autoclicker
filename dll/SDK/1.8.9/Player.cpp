#include "Player.h"
#include "Mappings.h"

Inventory Player::getInventory()
{
	static jfieldID f = nullptr;
	JField(f, this->GetClass(), FLD_Player_inventory, DESC_Player_inventory);
	if (!f) { lc->env->ExceptionClear(); return Inventory(nullptr); }
	jobject rtn = lc->env->GetObjectField(this->GetInstance(), f);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return Inventory(nullptr); }
	return Inventory(rtn);
}
