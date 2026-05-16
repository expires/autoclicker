#include "Camera.h"
#include "Mappings.h"

jclass Camera::GetClass() { return lc->GetClass(MC_Camera); }

Vec3 Camera::getPosition()
{
    jfieldID f = lc->env->GetFieldID(this->GetClass(), FLD_Camera_position, DESC_Camera_position);
    jobject  v = lc->env->GetObjectField(this->instance, f);
    return Vec3(v);
}

float Camera::getXRot()
{
    jfieldID f = lc->env->GetFieldID(this->GetClass(), FLD_Camera_xRot, "F");
    return lc->env->GetFloatField(this->instance, f);
}

float Camera::getYRot()
{
    jfieldID f = lc->env->GetFieldID(this->GetClass(), FLD_Camera_yRot, "F");
    return lc->env->GetFloatField(this->instance, f);
}
