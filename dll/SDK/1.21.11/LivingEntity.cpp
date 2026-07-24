#include "LivingEntity.h"
#include "Mappings.h"

ItemStack LivingEntity::getItemInHand()
{
	static jmethodID getItemInHandMethod = nullptr;
	JMethod(getItemInHandMethod, this->GetClass(), MTD_LivingEntity_getItemInHand, DESC_LivingEntity_getItemInHand);

	static jclass interactionHandClass = nullptr;
	static jfieldID mainHandField = nullptr;
	JStaticField(mainHandField, JClass(interactionHandClass, MC_InteractionHand),
		FLD_InteractionHand_MAIN, DESC_InteractionHand_MAIN);
	jobject mainHand = lc->env->GetStaticObjectField(interactionHandClass, mainHandField);

	jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), getItemInHandMethod, mainHand);

	if (rtn == nullptr)
		return nullptr;

	return ItemStack(rtn);
}

ItemStack LivingEntity::getArmorItem(int index)
{
	if (index < 0 || index > 3) return ItemStack(nullptr);

	static jmethodID getItemBySlot = nullptr;
	JMethod(getItemBySlot, this->GetClass(), MTD_LivingEntity_getItemBySlot, DESC_LivingEntity_getItemBySlot);
	if (!getItemBySlot) { lc->env->ExceptionClear(); return ItemStack(nullptr); }

	static jclass   slotClass = nullptr;
	static jfieldID slotFields[4] = { nullptr, nullptr, nullptr, nullptr };
	static const char* slotNames[4] = {
		FLD_EquipmentSlot_HEAD, FLD_EquipmentSlot_CHEST,
		FLD_EquipmentSlot_LEGS, FLD_EquipmentSlot_FEET
	};
	JStaticField(slotFields[index], JClass(slotClass, MC_EquipmentSlot),
		slotNames[index], DESC_EquipmentSlot);
	if (!slotFields[index]) { lc->env->ExceptionClear(); return ItemStack(nullptr); }

	jobject slot = lc->env->GetStaticObjectField(slotClass, slotFields[index]);
	if (slot == nullptr) { lc->env->ExceptionClear(); return ItemStack(nullptr); }

	jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), getItemBySlot, slot);
	lc->env->DeleteLocalRef(slot);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return ItemStack(nullptr); }
	if (rtn == nullptr) return ItemStack(nullptr);

	return ItemStack(rtn);
}
