#include "Vec3.h"
#include "Mappings.h"

jclass Vec3::GetClass() { static jclass c = nullptr; return JClass(c, MC_Vec3); }

double Vec3::getX()
{
    static jfieldID f = nullptr;
    if (!JField(f, GetClass(), FLD_Vec3_x, "D")) return 0.0;
    return lc->env->GetDoubleField(this->instance, f);
}
double Vec3::getY()
{
    static jfieldID f = nullptr;
    if (!JField(f, GetClass(), FLD_Vec3_y, "D")) return 0.0;
    return lc->env->GetDoubleField(this->instance, f);
}
double Vec3::getZ()
{
    static jfieldID f = nullptr;
    if (!JField(f, GetClass(), FLD_Vec3_z, "D")) return 0.0;
    return lc->env->GetDoubleField(this->instance, f);
}
