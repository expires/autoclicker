#include "MultiPlayerGameMode.h"
#include "Mappings.h"

int MultiPlayerGameMode::getPlayerMode()
{
	static jmethodID getPlayerMode = nullptr;
	JMethod(getPlayerMode, this->GetClass(), MTD_MPGM_getPlayerMode, DESC_MPGM_getPlayerMode);
	if (!getPlayerMode) { lc->env->ExceptionClear(); return -1; }
	jobject typeObj = lc->env->CallObjectMethod(this->GetInstance(), getPlayerMode);
	if (!typeObj || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return -1; }

	static jclass typeClass = nullptr;
	static jmethodID ordinalMethod = nullptr;
	if (!typeClass) JClass(typeClass, MC_GameType);
	JMethod(ordinalMethod, typeClass, "ordinal", "()I");
	if (!ordinalMethod) { lc->env->ExceptionClear(); return -1; }
	int r = lc->env->CallIntMethod(typeObj, ordinalMethod);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return -1; }
	return r;
}
