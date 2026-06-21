#include "LivingEntity.h"
#include "Mappings.h"

ItemStack LivingEntity::getItemInHand()
{
	static jmethodID m = nullptr;
	JMethod(m, this->GetClass(), MTD_LivingEntity_getItemInHand, DESC_LivingEntity_getItemInHand);
	if (!m) { lc->env->ExceptionClear(); return nullptr; }
	jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), m);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return nullptr; }
	if (rtn == nullptr) return nullptr;
	return ItemStack(rtn);
}
