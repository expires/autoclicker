#include "Minecraft.h"
#include "Mappings.h"

DeltaTracker Minecraft::GetDeltaTracker()
{
	static jmethodID m = nullptr;
	JMethod(m, this->GetClass(), MTD_Minecraft_getDeltaTracker, DESC_Minecraft_getDeltaTracker);
	if (!m) { lc->env->ExceptionClear(); return DeltaTracker(nullptr); }
	jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), m);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return DeltaTracker(nullptr); }
	return DeltaTracker(rtn);
}
