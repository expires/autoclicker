#include "LivingEntity.h"
#include "Mappings.h"

jclass LivingEntity::GetClass()
{
    return lc->GetClass(MC_LivingEntity);
}

ItemStack LivingEntity::getItemInHand()
{
	jmethodID getItemInHandMethod = lc->env->GetMethodID(this->GetClass(),
		MTD_LivingEntity_getItemInHand, DESC_LivingEntity_getItemInHand);

	jclass interactionHandClass = lc->GetClass(MC_InteractionHand);
	jfieldID mainHandField = lc->env->GetStaticFieldID(interactionHandClass,
		FLD_InteractionHand_MAIN, DESC_InteractionHand_MAIN);
	jobject mainHand = lc->env->GetStaticObjectField(interactionHandClass, mainHandField);

	jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), getItemInHandMethod, mainHand);

	if (rtn == nullptr)
		return nullptr;

	return ItemStack(rtn);
}

bool LivingEntity::isUsingItem()
{
	jmethodID isUsingItem = lc->env->GetMethodID(this->GetClass(),
		MTD_LivingEntity_isUsingItem, "()Z");
	return lc->env->CallBooleanMethod(this->GetInstance(), isUsingItem);
}

float LivingEntity::getHealth()
{
	jmethodID m = lc->env->GetMethodID(this->GetClass(),
		MTD_LivingEntity_getHealth, DESC_LivingEntity_getHealth);
	return lc->env->CallFloatMethod(this->GetInstance(), m);
}

float LivingEntity::getMaxHealth()
{
	jmethodID m = lc->env->GetMethodID(this->GetClass(),
		MTD_LivingEntity_getMaxHealth, DESC_LivingEntity_getMaxHealth);
	return lc->env->CallFloatMethod(this->GetInstance(), m);
}
