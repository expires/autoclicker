#include "AimAssistModule.h"
#include "../../Settings.h"
#include "../../SDK/Minecraft.h"
#include "../autoclicker/AutoclickerModule.h"
#include "../../overlay/Overlay.h"
#include <chrono>
#include <cmath>
#include <thread>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace AimAssistModule
{
    static bool jvmReady()
    {
        return lc->vm != nullptr && lc->classesLoaded.load(std::memory_order_acquire);
    }

    // MC's yaw convention: 0 = facing +Z, increases counter-clockwise around
    // +Y. Pitch: positive looks down. Inputs are world-space deltas from the
    // local player's eye to the target point; outputs are in degrees.
    static void anglesTo(double dx, double dy, double dz, float& outYaw, float& outPitch)
    {
        const double horiz = std::sqrt(dx * dx + dz * dz);
        outYaw   = (float)(std::atan2(dz, dx) * 180.0 / M_PI - 90.0);
        outPitch = (float)(-std::atan2(dy, horiz) * 180.0 / M_PI);
    }

    static float wrapDeg(float d)
    {
        while (d >  180.f) d -= 360.f;
        while (d < -180.f) d += 360.f;
        return d;
    }

    // Push a raw mouse delta into the OS input stream. SendInput injected
    // events show up in the WM_INPUT raw-input stream, which is what GLFW's
    // win32 backend listens to in cursor-disabled mode — so this composes
    // additively with whatever the user is moving in the same frame.
    static void injectMouseMove(int dx, int dy)
    {
        if (dx == 0 && dy == 0) return;
        INPUT in        = {};
        in.type         = INPUT_MOUSE;
        in.mi.dx        = dx;
        in.mi.dy        = dy;
        in.mi.dwFlags   = MOUSEEVENTF_MOVE;
        SendInput(1, &in, sizeof(INPUT));
    }

    DWORD WINAPI init(LPVOID /*lpParam*/)
    {
        // Wait for the autoclicker thread to attach + populate the class map.
        while (!AutoclickerModule::destruct && !jvmReady())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (AutoclickerModule::destruct) return 0;

        if (lc->vm->AttachCurrentThread(reinterpret_cast<void**>(&lc->env), nullptr) != JNI_OK)
            return 0;
        if (lc->env == nullptr) return 0;

        Minecraft  mc;
        const HWND mcWindow = FindWindowW(L"GLFW30", nullptr);
        if (mcWindow == nullptr) {
            lc->vm->DetachCurrentThread();
            return 0;
        }

        while (!AutoclickerModule::destruct)
        {
            // 100Hz — fast enough to feel smooth without burning measurable CPU
            // on the JNI scan + a handful of math ops per player.
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            if (!g_settings.aimEnabled)             continue;
            if (Overlay::IsMenuVisible())           continue;
            if (GetForegroundWindow() != mcWindow)  continue;
            if (g_settings.aimClickOnly &&
                !(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) continue;

            // Bound JNI local refs to this iteration so the per-player team /
            // position refs don't leak across ticks.
            if (lc->env->PushLocalFrame(128) != 0) {
                lc->env->ExceptionClear();
                continue;
            }

            Player local = mc.GetLocalPlayer();
            if (local.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); continue; }

            Level level = mc.GetLevel();
            if (level.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); continue; }

            Vec3 lpos = local.getPosition();
            if (lpos.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); continue; }

            // Eye position. 1.62 is the standing eye height; sneak/swim alter
            // it (1.27 / 0.4 respectively) but we don't bother — the assist is
            // a closed loop and self-corrects on the next tick if the eye Y
            // estimate is slightly off.
            const double ex       = lpos.getX();
            const double ey       = lpos.getY() + 1.62;
            const double ez       = lpos.getZ();
            const float  curYaw   = local.getYRot();
            const float  curPitch = local.getXRot();

            // Snapshot the scoreboard team. PlayerTeams are singletons per
            // team name, so JNI ref-equality is the same as name-equality.
            const jobject myTeam = local.getTeamRaw();

            auto players = level.players();
            const jobject localInst = local.GetInstance();
            const double  maxDistSq = (double)g_settings.aimRange * (double)g_settings.aimRange;
            const float   halfFov   = g_settings.aimFov * 0.5f;

            // Pick the target by smallest angular distance to crosshair within
            // the FOV cone. Ranks by combined yaw² + pitch² since aim assist
            // should follow where you're looking, not which player is closest
            // in world distance.
            float bestAng2       = halfFov * halfFov;
            float bestYawDelta   = 0.f;
            float bestPitchDelta = 0.f;
            bool  haveTarget     = false;

            for (auto& p : players)
            {
                if (lc->env->IsSameObject(p.GetInstance(), localInst)) continue;

                // Same-team skip. If I have no team, never skip — vanilla MC
                // doesn't treat "no team" as an alliance and neither do we.
                if (myTeam != nullptr) {
                    jobject pt = p.getTeamRaw();
                    if (pt != nullptr) {
                        const bool same = (lc->env->IsSameObject(pt, myTeam) == JNI_TRUE);
                        lc->env->DeleteLocalRef(pt);
                        if (same) continue;
                    }
                }

                Vec3 ppos = p.getPosition();
                if (ppos.GetInstance() == nullptr) continue;
                const double tx = ppos.getX();
                const double ty_feet = ppos.getY();
                const double tz = ppos.getZ();

                const double ddx = tx - ex;
                const double ddz = tz - ez;
                const double horizSq = ddx * ddx + ddz * ddz;
                if (horizSq > maxDistSq) continue;

                // Where on the target to aim. 0.9 is roughly the AABB center
                // for a standing player (1.8m tall), 1.62 is head/eye height.
                double ty = ty_feet;
                if      (g_settings.aimTargetPart == 1) ty += 0.9;
                else if (g_settings.aimTargetPart == 2) ty += 1.62;

                float wantYaw, wantPitch;
                anglesTo(ddx, ty - ey, ddz, wantYaw, wantPitch);

                const float dy_ang = wrapDeg(wantYaw   - curYaw);
                const float dp_ang =          wantPitch - curPitch;
                const float ang2   = dy_ang * dy_ang + dp_ang * dp_ang;
                if (ang2 > bestAng2) continue;

                bestAng2       = ang2;
                bestYawDelta   = dy_ang;
                bestPitchDelta = dp_ang;
                haveTarget     = true;
            }

            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
            lc->env->PopLocalFrame(nullptr);

            if (!haveTarget) continue;

            // Per-tick step toward target. Quadratic so the low end of the
            // 0-10 slider feels gentle (1 → 1% of remaining per tick) and the
            // top is snappy (10 → 10% of remaining per tick). At 100Hz that
            // gives a half-life of ~70ms at slider=10, ~7s at slider=1.
            const float fH = (float)g_settings.aimSpeedH / 10.0f;
            const float fV = (float)g_settings.aimSpeedV / 10.0f;
            float stepYaw   = bestYawDelta   * fH * fH * 0.10f;
            float stepPitch = bestPitchDelta * fV * fV * 0.10f;

            // Cap the per-tick step so a far-off target doesn't translate to
            // a one-shot snap (visually obvious + can overshoot through the
            // target with momentum).
            constexpr float MAX_STEP_DEG = 8.0f;
            if (stepYaw   >  MAX_STEP_DEG) stepYaw   =  MAX_STEP_DEG;
            if (stepYaw   < -MAX_STEP_DEG) stepYaw   = -MAX_STEP_DEG;
            if (stepPitch >  MAX_STEP_DEG) stepPitch =  MAX_STEP_DEG;
            if (stepPitch < -MAX_STEP_DEG) stepPitch = -MAX_STEP_DEG;

            // Convert degrees → raw mouse pixels. The constant is the rough
            // factor for default MC sensitivity (1.0). Other sensitivities
            // change the gain, which the speed slider absorbs; the closed
            // loop self-corrects either way (next tick re-reads the new yaw).
            constexpr float PIXELS_PER_DEGREE = 8.0f;
            const int pxX = (int)std::lround(stepYaw   * PIXELS_PER_DEGREE);
            const int pxY = (int)std::lround(stepPitch * PIXELS_PER_DEGREE);

            injectMouseMove(pxX, pxY);
        }

        lc->vm->DetachCurrentThread();
        return 0;
    }
}
