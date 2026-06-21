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
