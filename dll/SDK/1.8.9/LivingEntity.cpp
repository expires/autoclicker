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

int LivingEntity::getLatency()
{
	jobject inst = this->GetInstance();
	if (inst == nullptr) return -1;

	static jclass acp = nullptr;
	JClass(acp, MC_AbstractClientPlayer);
	if (!acp) { lc->env->ExceptionClear(); return -1; }

	static jmethodID getInfo = nullptr;
	JMethod(getInfo, acp, MTD_AbstractClientPlayer_getPlayerInfo, DESC_AbstractClientPlayer_getPlayerInfo);
	if (!getInfo) { lc->env->ExceptionClear(); return -1; }

	jobject info = lc->env->CallObjectMethod(inst, getInfo);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return -1; }
	if (info == nullptr) return -1;

	static jmethodID getLat = nullptr;
	if (!getLat) {
		jclass ic = lc->env->GetObjectClass(info);
		JMethod(getLat, ic, MTD_PlayerInfo_getLatency, "()I");
		lc->env->DeleteLocalRef(ic);
	}
	if (!getLat) { lc->env->DeleteLocalRef(info); lc->env->ExceptionClear(); return -1; }

	jint lat = lc->env->CallIntMethod(info, getLat);
	lc->env->DeleteLocalRef(info);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return -1; }
	return (int)lat;
}
