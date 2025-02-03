#include "Player.h"

Player::Player(jobject instance)
{
	this->instance = instance;
}

jclass Player::GetClass()
{
	return lc->GetClass("net.minecraft.client.player.LocalPlayer");
}

void Player::Cleanup()
{
	lc->env->DeleteLocalRef(this->instance);
}

jobject Player::GetInstance()
{
	return this->instance;
}

ItemStack Player::getItemInHand()
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

bool Player::isUsingItem()
{
	jmethodID isUsingItem = lc->env->GetMethodID(this->GetClass(), "isUsingItem", "()Z");
	bool rtn = lc->env->CallBooleanMethod(this->GetInstance(), isUsingItem);

	return rtn;
}