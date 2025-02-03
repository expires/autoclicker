#include "LivingEntity.h"

jclass LivingEntity::GetClass()
{
    return lc->GetClass("net.minecraft.world.entity.LivingEntity");
}

ItemStack LivingEntity::getItemInHand()
{

	jmethodID getItemInHandMethod = lc->env->GetMethodID(this->GetClass(), "getItemInHand", "(Lnet/minecraft/world/InteractionHand;)Lnet/minecraft/world/item/ItemStack;");

	jclass interactionHandClass = lc->GetClass("net.minecraft.world.InteractionHand");
	jfieldID mainHandField = lc->env->GetStaticFieldID(interactionHandClass, "MAIN_HAND", "Lnet/minecraft/world/InteractionHand;");
	jobject mainHand = lc->env->GetStaticObjectField(interactionHandClass, mainHandField);

	jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), getItemInHandMethod, mainHand);

	if (rtn == nullptr)
		return nullptr;

	return ItemStack(rtn);
}

bool LivingEntity::isUsingItem()
{
	jmethodID isUsingItem = lc->env->GetMethodID(this->GetClass(), "isUsingItem", "()Z");
	bool rtn = lc->env->CallBooleanMethod(this->GetInstance(), isUsingItem);

	return rtn;
}