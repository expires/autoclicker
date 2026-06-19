#include "LivingEntity.h"
#include "Mappings.h"

jclass LivingEntity::GetClass()
{
    static jclass c = nullptr;
    return JClass(c, MC_LivingEntity);
}

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

bool LivingEntity::isUsingItem()
{
	static jmethodID isUsingItem = nullptr;
	JMethod(isUsingItem, this->GetClass(), MTD_LivingEntity_isUsingItem, "()Z");
	return lc->env->CallBooleanMethod(this->GetInstance(), isUsingItem);
}

float LivingEntity::getHealth()
{
	static jmethodID m = nullptr;
	JMethod(m, this->GetClass(), MTD_LivingEntity_getHealth, "()F");
	if (!m) { lc->env->ExceptionClear(); return -1.0f; }
	jfloat v = lc->env->CallFloatMethod(this->GetInstance(), m);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return -1.0f; }
	return v;
}

float LivingEntity::getMaxHealth()
{
	static jmethodID m = nullptr;
	JMethod(m, this->GetClass(), MTD_LivingEntity_getMaxHealth, "()F");
	if (!m) { lc->env->ExceptionClear(); return -1.0f; }
	jfloat v = lc->env->CallFloatMethod(this->GetInstance(), m);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return -1.0f; }
	return v;
}
