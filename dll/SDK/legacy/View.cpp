#include "View.h"
#include "Mappings.h"

ViewState AcquireView(Minecraft& mc, Player& localPlayer)
{
    ViewState v;
    v.gotRenderer = true;
    v.gotCamera   = true;

    jobject mcInst = mc.GetInstance();

    float pt = 1.0f;
    if (mcInst != nullptr)
    {
        static jfieldID timerF = nullptr;
        JField(timerF, mc.GetClass(), FLD_Minecraft_timer, DESC_Minecraft_timer);
        if (timerF)
        {
            jobject timer = lc->env->GetObjectField(mcInst, timerF);
            if (timer != nullptr && !lc->env->ExceptionCheck())
            {
                static jclass    timerCls = nullptr;
                static jfieldID  rptF     = nullptr;
                if (!timerCls) JClass(timerCls, MC_Timer);
                JField(rptF, timerCls, FLD_Timer_renderPartialTicks, "F");
                if (rptF)
                {
                    jfloat r = lc->env->GetFloatField(timer, rptF);
                    if (!lc->env->ExceptionCheck() && r >= 0.0f && r <= 1.0f) pt = r;
                }
                lc->env->DeleteLocalRef(timer);
            }
            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        }
    }
    v.partialTick = pt;

    const double x  = localPlayer.getX();
    const double y  = localPlayer.getY();
    const double z  = localPlayer.getZ();
    const double px = localPlayer.getXo();
    const double py = localPlayer.getYo();
    const double pz = localPlayer.getZo();
    v.x = px + (x - px) * pt;
    v.y = py + (y - py) * pt + 1.62;
    v.z = pz + (z - pz) * pt;

    const float yaw   = localPlayer.getYRot();
    const float pitch = localPlayer.getXRot();
    float pYaw = yaw, pPitch = pitch;
    {
        static jfieldID pyF = nullptr;
        static jfieldID ppF = nullptr;
        if (JField(pyF, localPlayer.GetClass(), FLD_Entity_prevYRot, "F"))
        {
            jfloat r = lc->env->GetFloatField(localPlayer.GetInstance(), pyF);
            if (!lc->env->ExceptionCheck()) pYaw = r;
        }
        if (JField(ppF, localPlayer.GetClass(), FLD_Entity_prevXRot, "F"))
        {
            jfloat r = lc->env->GetFloatField(localPlayer.GetInstance(), ppF);
            if (!lc->env->ExceptionCheck()) pPitch = r;
        }
        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
    }
    v.yRot = pYaw + (yaw - pYaw) * pt;
    v.xRot = pPitch + (pitch - pPitch) * pt;

    v.fov = 70.0f;
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
