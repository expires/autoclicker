#include "GameRenderer.h"
#include "Mappings.h"

jclass GameRenderer::GetClass() { return lc->GetClass(MC_GameRenderer); }

Camera GameRenderer::getMainCamera()
{
    jfieldID f = lc->env->GetFieldID(this->GetClass(),
        FLD_GameRenderer_mainCamera, DESC_GameRenderer_mainCamera);
    jobject c = lc->env->GetObjectField(this->instance, f);
    return Camera(c);
}

float GameRenderer::getFov(Camera cam, float partialTicks, bool useFOVSetting)
{
    jmethodID m = lc->env->GetMethodID(this->GetClass(),
        MTD_GameRenderer_getFov, DESC_GameRenderer_getFov);
    if (!m)
    {
        lc->env->ExceptionClear();
        return 70.0f;
    }
    return lc->env->CallFloatMethod(this->instance, m,
        cam.GetInstance(), partialTicks, (jboolean)useFOVSetting);
}
