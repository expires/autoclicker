#include "AABB.h"
#include "Mappings.h"

jclass AABB::GetClass() { static jclass c = nullptr; return JClass(c, MC_AABB); }

static double read(jfieldID& slot, jclass cls, jobject inst, const char* name)
{
    if (!JField(slot, cls, name, "D")) return 0.0;
    return lc->env->GetDoubleField(inst, slot);
}

double AABB::minX() { static jfieldID f = nullptr; return read(f, GetClass(), instance, FLD_AABB_minX); }
double AABB::minY() { static jfieldID f = nullptr; return read(f, GetClass(), instance, FLD_AABB_minY); }
double AABB::minZ() { static jfieldID f = nullptr; return read(f, GetClass(), instance, FLD_AABB_minZ); }
double AABB::maxX() { static jfieldID f = nullptr; return read(f, GetClass(), instance, FLD_AABB_maxX); }
double AABB::maxY() { static jfieldID f = nullptr; return read(f, GetClass(), instance, FLD_AABB_maxY); }
double AABB::maxZ() { static jfieldID f = nullptr; return read(f, GetClass(), instance, FLD_AABB_maxZ); }
