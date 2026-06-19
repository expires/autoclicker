#include "GameRenderer.h"
#include "Mappings.h"

jclass GameRenderer::GetClass() { static jclass c = nullptr; return JClass(c, MC_GameRenderer); }

Camera GameRenderer::getMainCamera()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_GameRenderer_mainCamera, DESC_GameRenderer_mainCamera)) return Camera(nullptr);
    jobject c = lc->env->GetObjectField(this->instance, f);
    return Camera(c);
}

float GameRenderer::getFov(Camera cam, float partialTicks, bool useFOVSetting)
{
    static jmethodID m = nullptr;
    JMethod(m, this->GetClass(), MTD_GameRenderer_getFov, DESC_GameRenderer_getFov);
    if (!m)
    {
        lc->env->ExceptionClear();
        return 70.0f;
    }
    return lc->env->CallFloatMethod(this->instance, m,
        cam.GetInstance(), partialTicks, (jboolean)useFOVSetting);
}
