#include "View.h"
#include "Mappings.h"
#include <cmath>

static bool readFloatBuffer(jobject buf, float* out, int n)
{
    if (buf == nullptr) return false;
    static jclass    fbCls = nullptr;
    static jmethodID getM  = nullptr;
    if (getM == nullptr)
    {
        jclass c = lc->env->GetObjectClass(buf);
        getM = lc->env->GetMethodID(c, "get", "(I)F");
        if (c) lc->env->DeleteLocalRef(c);
        if (getM == nullptr) { lc->env->ExceptionClear(); return false; }
    }
    for (int i = 0; i < n; ++i)
    {
        out[i] = lc->env->CallFloatMethod(buf, getM, (jint)i);
        if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return false; }
    }
    return true;
}

static bool readIntBuffer(jobject buf, int* out, int n)
{
    if (buf == nullptr) return false;
    static jclass    ibCls = nullptr;
    static jmethodID getM  = nullptr;
    if (getM == nullptr)
    {
        jclass c = lc->env->GetObjectClass(buf);
        getM = lc->env->GetMethodID(c, "get", "(I)I");
        if (c) lc->env->DeleteLocalRef(c);
        if (getM == nullptr) { lc->env->ExceptionClear(); return false; }
    }
    for (int i = 0; i < n; ++i)
    {
        out[i] = (int)lc->env->CallIntMethod(buf, getM, (jint)i);
        if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return false; }
    }
    return true;
}

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
    v.y = py + (y - py) * pt;
    v.z = pz + (z - pz) * pt;

    v.yRot = localPlayer.getYRot();
    v.xRot = localPlayer.getXRot();
    v.fov  = 70.0f;

    static jclass ariCls = nullptr;
    if (!ariCls) JClass(ariCls, MC_ActiveRenderInfo);
    if (ariCls)
    {
        static jfieldID mvF = nullptr, prF = nullptr, vpF = nullptr;
        JStaticField(mvF, ariCls, FLD_ActiveRenderInfo_modelview,  DESC_ActiveRenderInfo_modelview);
        JStaticField(prF, ariCls, FLD_ActiveRenderInfo_projection, DESC_ActiveRenderInfo_projection);
        JStaticField(vpF, ariCls, FLD_ActiveRenderInfo_viewport,   DESC_ActiveRenderInfo_viewport);
        if (mvF && prF && vpF)
        {
            jobject mvBuf = lc->env->GetStaticObjectField(ariCls, mvF);
            jobject prBuf = lc->env->GetStaticObjectField(ariCls, prF);
            jobject vpBuf = lc->env->GetStaticObjectField(ariCls, vpF);
            if (!lc->env->ExceptionCheck()
                && readFloatBuffer(mvBuf, v.modelview, 16)
                && readFloatBuffer(prBuf, v.projection, 16)
                && readIntBuffer(vpBuf, v.viewport, 4))
            {
                v.hasMatrix = true;
                if (v.projection[5] > 0.0001f)
                    v.fov = (float)(2.0 * std::atan(1.0 / (double)v.projection[5]) * 180.0 / 3.14159265358979323846);
            }
            if (mvBuf) lc->env->DeleteLocalRef(mvBuf);
            if (prBuf) lc->env->DeleteLocalRef(prBuf);
            if (vpBuf) lc->env->DeleteLocalRef(vpBuf);
        }
        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
    }

    v.ok = true;
    return v;
}
