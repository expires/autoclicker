#include "View.h"
#include "Mappings.h"

ViewState AcquireView(Minecraft& mc, Player& localPlayer)
{
    ViewState v;

    GameRenderer gr = mc.GetGameRenderer();
    if (gr.GetInstance() == nullptr) return v;
    v.gotRenderer = true;

    Camera cam = gr.getMainCamera();
    if (cam.GetInstance() == nullptr) return v;
    v.gotCamera = true;

    DeltaTracker dt = mc.GetDeltaTracker();
    v.partialTick = dt.GetInstance() != nullptr ? dt.getPartialTick(true) : 1.0f;

    Vec3 camPos = cam.getPosition();
    v.x    = camPos.getX();
    v.y    = camPos.getY();
    v.z    = camPos.getZ();
    v.yRot = cam.getYRot();
    v.xRot = cam.getXRot();
    v.fov  = gr.getFov(cam, v.partialTick, true);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); v.fov = 70.0f; }

    v.ok = true;
    return v;
}
