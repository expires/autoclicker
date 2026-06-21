#include "Camera.h"
#include "Mappings.h"

jclass Camera::GetClass() { static jclass c = nullptr; return JClass(c, MC_Camera); }

Vec3 Camera::getPosition()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_Camera_position, DESC_Camera_position)) return Vec3(nullptr);
    jobject  v = lc->env->GetObjectField(this->instance, f);
    return Vec3(v);
}

float Camera::getXRot()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_Camera_xRot, "F")) return 0.0f;
    return lc->env->GetFloatField(this->instance, f);
}

float Camera::getYRot()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_Camera_yRot, "F")) return 0.0f;
    return lc->env->GetFloatField(this->instance, f);
}

float Camera::getFov()
{
    static jmethodID m = nullptr;
    if (!JMethod(m, this->GetClass(), MTD_Camera_getFov, DESC_Camera_getFov)) { lc->env->ExceptionClear(); return -1.0f; }
    float v = lc->env->CallFloatMethod(this->instance, m);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return -1.0f; }
    return v;
}
