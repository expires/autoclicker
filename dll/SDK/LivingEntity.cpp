#include "LivingEntity.h"

jclass LivingEntity::GetClass()
{
    return lc->GetClass("net.minecraft.class_1309");
}

ItemStack LivingEntity::getItemInHand()
{

	jmethodID getItemInHandMethod = lc->env->GetMethodID(this->GetClass(), "method_24520", "(Lnet/minecraft/class_1268;)Lnet/minecraft/class_1799;");

	jclass interactionHandClass = lc->GetClass("net.minecraft.class_1268");
	jfieldID mainHandField = lc->env->GetStaticFieldID(interactionHandClass, "field_5808", "Lnet/minecraft/class_1268;");
	jobject mainHand = lc->env->GetStaticObjectField(interactionHandClass, mainHandField);

	jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), getItemInHandMethod, mainHand);

	if (rtn == nullptr)
		return nullptr;

	return ItemStack(rtn);
}

bool LivingEntity::isUsingItem()
{
	jmethodID isUsingItem = lc->env->GetMethodID(this->GetClass(), "method_6115", "()Z");
	bool rtn = lc->env->CallBooleanMethod(this->GetInstance(), isUsingItem);

	return rtn;
}