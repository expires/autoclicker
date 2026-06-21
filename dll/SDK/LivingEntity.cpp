#include "LivingEntity.h"
#include "Mappings.h"

jclass LivingEntity::GetClass()
{
    static jclass c = nullptr;
    return JClass(c, MC_LivingEntity);
}

bool LivingEntity::isUsingItem()
{
	static jmethodID isUsingItem = nullptr;
	JMethod(isUsingItem, this->GetClass(), MTD_LivingEntity_isUsingItem, "()Z");
	if (!isUsingItem) { lc->env->ExceptionClear(); return false; }
	jboolean v = lc->env->CallBooleanMethod(this->GetInstance(), isUsingItem);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return false; }
	return v == JNI_TRUE;
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
