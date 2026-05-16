#include "Vec3.h"
#include "Mappings.h"

jclass Vec3::GetClass() { return lc->GetClass(MC_Vec3); }

double Vec3::getX()
{
    jfieldID f = lc->env->GetFieldID(this->GetClass(), FLD_Vec3_x, "D");
    return lc->env->GetDoubleField(this->instance, f);
}
double Vec3::getY()
{
    jfieldID f = lc->env->GetFieldID(this->GetClass(), FLD_Vec3_y, "D");
    return lc->env->GetDoubleField(this->instance, f);
}
double Vec3::getZ()
{
    jfieldID f = lc->env->GetFieldID(this->GetClass(), FLD_Vec3_z, "D");
    return lc->env->GetDoubleField(this->instance, f);
}
