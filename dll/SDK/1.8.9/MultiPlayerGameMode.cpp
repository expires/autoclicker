#include "MultiPlayerGameMode.h"
#include "Mappings.h"

int MultiPlayerGameMode::getPlayerMode()
{
	static jmethodID getCurrentGameType = nullptr;
	JMethod(getCurrentGameType, this->GetClass(), MTD_MPGM_getPlayerMode, DESC_MPGM_getPlayerMode);
	if (!getCurrentGameType) { lc->env->ExceptionClear(); return -1; }
	jobject typeObj = lc->env->CallObjectMethod(this->GetInstance(), getCurrentGameType);
	if (!typeObj || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return -1; }

	static jclass typeClass = nullptr;
	static jmethodID getIdMethod = nullptr;
	if (!typeClass) JClass(typeClass, MC_GameType);
	JMethod(getIdMethod, typeClass, MTD_GameType_getId, "()I");
	if (!getIdMethod) { lc->env->ExceptionClear(); return -1; }
	int r = lc->env->CallIntMethod(typeObj, getIdMethod);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return -1; }
	return r;
}
