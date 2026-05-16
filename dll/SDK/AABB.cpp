#include "AABB.h"
#include "Mappings.h"

jclass AABB::GetClass() { return lc->GetClass(MC_AABB); }

static double read(jclass cls, jobject inst, const char* name)
{
    jfieldID f = lc->env->GetFieldID(cls, name, "D");
    return lc->env->GetDoubleField(inst, f);
}

double AABB::minX() { return read(GetClass(), instance, FLD_AABB_minX); }
double AABB::minY() { return read(GetClass(), instance, FLD_AABB_minY); }
double AABB::minZ() { return read(GetClass(), instance, FLD_AABB_minZ); }
double AABB::maxX() { return read(GetClass(), instance, FLD_AABB_maxX); }
double AABB::maxY() { return read(GetClass(), instance, FLD_AABB_maxY); }
double AABB::maxZ() { return read(GetClass(), instance, FLD_AABB_maxZ); }
