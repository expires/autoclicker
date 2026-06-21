#include "View.h"
#include "Mappings.h"

ViewState AcquireView(Minecraft& mc, Player& localPlayer)
{
    ViewState v;

    v.gotRenderer = true;
    v.gotCamera   = true;
    v.partialTick = 1.0f;

    v.x    = localPlayer.getX();
    v.y    = localPlayer.getY() + 1.62;
    v.z    = localPlayer.getZ();
    v.yRot = localPlayer.getYRot();
    v.xRot = localPlayer.getXRot();
    v.fov  = 70.0f;

    jobject mcInst = mc.GetInstance();
    if (mcInst != nullptr)
    {
        static jfieldID gsF = nullptr;
        JField(gsF, mc.GetClass(), FLD_Minecraft_gameSettings, DESC_Minecraft_gameSettings);
        if (gsF)
        {
            jobject gs = lc->env->GetObjectField(mcInst, gsF);
            if (gs != nullptr && !lc->env->ExceptionCheck())
            {
                static jclass    gsCls = nullptr;
                static jfieldID  fovF  = nullptr;
                if (!gsCls) JClass(gsCls, MC_GameSettings);
                JField(fovF, gsCls, FLD_GameSettings_fovSetting, "F");
                if (fovF)
                {
                    jfloat fvVal = lc->env->GetFloatField(gs, fovF);
                    if (!lc->env->ExceptionCheck() && fvVal > 1.0f) v.fov = fvVal;
                }
                lc->env->DeleteLocalRef(gs);
            }
            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        }
    }

    v.ok = true;
    return v;
}
