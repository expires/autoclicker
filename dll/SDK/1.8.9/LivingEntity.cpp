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

ItemStack LivingEntity::getArmorItem(int index)
{
	if (index < 0 || index > 3) return ItemStack(nullptr);

	static jmethodID getEquipment = nullptr;
	JMethod(getEquipment, this->GetClass(), MTD_LivingEntity_getEquipmentInSlot, DESC_LivingEntity_getEquipmentInSlot);
	if (!getEquipment) { lc->env->ExceptionClear(); return ItemStack(nullptr); }

	const int slot = 4 - index;
	jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), getEquipment, (jint)slot);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return ItemStack(nullptr); }
	if (rtn == nullptr) return ItemStack(nullptr);

	return ItemStack(rtn);
}
