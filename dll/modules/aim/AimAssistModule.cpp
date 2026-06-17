#include "AimAssistModule.h"
#include "../../Settings.h"
#include "../../SDK/Minecraft.h"
#include "../../SDK/BlockPos.h"
#include "../../SDK/BlockState.h"
#include "../autoclicker/AutoclickerModule.h"
#include "../../overlay/Overlay.h"
#include "../../Logger.h"
#include <cctype>
#include <cfloat>
#include <chrono>
#include <climits>
#include <cmath>
#include <string>
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

    static bool isLineOfSightClear(Level& level,
                                   double sx, double sy, double sz,
                                   double tx, double ty, double tz)
    {
        const double dx = tx - sx, dy = ty - sy, dz = tz - sz;
        const double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (dist < 1e-3) return true;
        const double inv = 1.0 / dist;
        const double ux = dx * inv, uy = dy * inv, uz = dz * inv;

        constexpr double STEP        = 0.30;
        constexpr double START_OFF   = 0.30;
        constexpr double END_OFF     = 0.40;
        const double endD = dist - END_OFF;
        if (endD <= START_OFF) return true;

        int lastBx = INT_MIN, lastBy = INT_MIN, lastBz = INT_MIN;
        for (double d = START_OFF; d <= endD; d += STEP) {
            const int bx = (int)std::floor(sx + ux * d);
            const int by = (int)std::floor(sy + uy * d);
            const int bz = (int)std::floor(sz + uz * d);

            if (bx == lastBx && by == lastBy && bz == lastBz) continue;
            lastBx = bx; lastBy = by; lastBz = bz;

            BlockPos bp = BlockPos::make(bx, by, bz);
            if (!bp.GetInstance()) continue;
            BlockState bs = level.getBlockState(bp);
            const bool solid = bs.blocksMotion();
            if (solid) return false;
        }
        return true;
    }

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

    static uint32_t teamColorOf(Entity& e)
    {
        auto chunks = e.getFormattedNameChunks();
        for (auto it = chunks.rbegin(); it != chunks.rend(); ++it)
            if (!it->first.empty()) return it->second;
        return 0xFFFFFFFFu;
    }

    DWORD WINAPI init(LPVOID )
    {
        AC_LOG("aim: thread start");
        while (!AutoclickerModule::destruct && !jvmReady())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (AutoclickerModule::destruct) return 0;

        if (lc->vm->AttachCurrentThread(reinterpret_cast<void**>(&lc->env), nullptr) != JNI_OK)
            return 0;
        if (lc->env == nullptr) return 0;
        AC_LOG("aim: attached; entering loop");

        Minecraft  mc;
        const HWND mcWindow = FindWindowW(L"GLFW30", nullptr);
        if (mcWindow == nullptr) {
            lc->vm->DetachCurrentThread();
            return 0;
        }

        while (!AutoclickerModule::destruct)
        {

            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            if (!g_settings.aimEnabled)             continue;
            if (Overlay::IsMenuVisible())           continue;
            if (GetForegroundWindow() != mcWindow)  continue;
            if (g_settings.aimClickOnly &&
                !(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) continue;

            if (lc->env->PushLocalFrame(256) != 0) {
                lc->env->ExceptionClear();
                continue;
            }

            if (mc.isPaused()) { lc->env->PopLocalFrame(nullptr); continue; }
            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
            Screen screen = mc.GetScreen();
            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
            if (screen.GetInstance() != nullptr) { lc->env->PopLocalFrame(nullptr); continue; }

            Player local = mc.GetLocalPlayer();
            if (local.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); continue; }

            Level level = mc.GetLevel();
            if (level.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); continue; }

            Vec3 lpos = local.getPosition();
            if (lpos.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); continue; }

            const double ex       = lpos.getX();
            const double ey       = lpos.getY() + 1.62;
            const double ez       = lpos.getZ();
            const float  curYaw   = local.getYRot();
            const float  curPitch = local.getXRot();

            const uint32_t myTeamColor =
                g_settings.teamsByColor ? teamColorOf(local) : 0xFFFFFFFFu;

            auto players = level.players();
            const jobject localInst = local.GetInstance();
            const double  maxDistSq = (double)g_settings.aimRange * (double)g_settings.aimRange;
            const float   halfFov   = g_settings.aimFov * 0.5f;

            float bestAng2       = halfFov * halfFov;
            float bestYawDelta   = 0.f;
            float bestPitchDelta = 0.f;
            bool  haveTarget     = false;

            for (auto& p : players)
            {
                if (lc->env->IsSameObject(p.GetInstance(), localInst)) continue;

                {
                    bool listEmpty;
                    {
                        std::lock_guard<std::mutex> lk(g_settings.friendsMutex);
                        listEmpty = g_settings.friends.empty();
                    }
                    if (!listEmpty) {
                        Component bare = p.getName();
                        if (bare.GetInstance() != nullptr) {
                            std::string name = bare.getString();
                            for (char& c : name)
                                c = (char)std::tolower((unsigned char)c);
                            bool isFriend = false;
                            {
                                std::lock_guard<std::mutex> lk(g_settings.friendsMutex);
                                for (const auto& f : g_settings.friends)
                                    if (f == name) { isFriend = true; break; }
                            }
                            if (isFriend) continue;
                        }
                    }
                }

                Vec3 ppos = p.getPosition();
                if (ppos.GetInstance() == nullptr) continue;
                const double tx = ppos.getX();
                const double tz = ppos.getZ();

                const double ddx0 = tx - ex;
                const double ddz0 = tz - ez;
                if (ddx0 * ddx0 + ddz0 * ddz0 > maxDistSq) continue;

                if (myTeamColor != 0xFFFFFFFFu && teamColorOf(p) == myTeamColor)
                    continue;

                AABB box = p.getBoundingBox();
                if (box.GetInstance() == nullptr) continue;
                const double minX = box.minX(), maxX = box.maxX();
                const double minY = box.minY(), maxY = box.maxY();
                const double minZ = box.minZ(), maxZ = box.maxZ();

                {
                    const double cxBody = (minX + maxX) * 0.5;
                    const double czBody = (minZ + maxZ) * 0.5;
                    const double bodyY  = (minY + maxY) * 0.5;
                    const double eyeY   = maxY - 0.18;
                    const double feetY  = minY + 0.20;
                    bool hasLos = false;
                    if (lc->env->PushLocalFrame(64) == 0) {
                        if (isLineOfSightClear(level, ex, ey, ez, cxBody, eyeY,  czBody) ||
                            isLineOfSightClear(level, ex, ey, ez, cxBody, bodyY, czBody) ||
                            isLineOfSightClear(level, ex, ey, ez, cxBody, feetY, czBody))
                            hasLos = true;
                        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                        lc->env->PopLocalFrame(nullptr);
                    }
                    if (!hasLos) continue;
                }

                constexpr double INNER_SHRINK = 0.55;
                const double cxMid = (minX + maxX) * 0.5;
                const double cyMid = (minY + maxY) * 0.5;
                const double czMid = (minZ + maxZ) * 0.5;
                const double hx    = (maxX - minX) * 0.5 * INNER_SHRINK;
                const double hy    = (maxY - minY) * 0.5 * INNER_SHRINK;
                const double hz    = (maxZ - minZ) * 0.5 * INNER_SHRINK;
                const double iMinX = cxMid - hx, iMaxX = cxMid + hx;
                const double iMinY = cyMid - hy, iMaxY = cyMid + hy;
                const double iMinZ = czMid - hz, iMaxZ = czMid + hz;

                float ymin = +FLT_MAX, ymax = -FLT_MAX;
                float pmin = +FLT_MAX, pmax = -FLT_MAX;
                for (int i = 0; i < 8; ++i) {
                    const double cx = (i & 1) ? iMaxX : iMinX;
                    const double cy = (i & 2) ? iMaxY : iMinY;
                    const double cz = (i & 4) ? iMaxZ : iMinZ;
                    float cy_ang, cp_ang;
                    anglesTo(cx - ex, cy - ey, cz - ez, cy_ang, cp_ang);
                    const float yawRel = curYaw + wrapDeg(cy_ang - curYaw);
                    if (yawRel < ymin) ymin = yawRel;
                    if (yawRel > ymax) ymax = yawRel;
                    if (cp_ang < pmin) pmin = cp_ang;
                    if (cp_ang > pmax) pmax = cp_ang;
                }

                float dy_ang = 0.f, dp_ang = 0.f;
                if      (curYaw   < ymin) dy_ang = ymin - curYaw;
                else if (curYaw   > ymax) dy_ang = ymax - curYaw;
                if      (curPitch < pmin) dp_ang = pmin - curPitch;
                else if (curPitch > pmax) dp_ang = pmax - curPitch;

                const float ang2 = dy_ang * dy_ang + dp_ang * dp_ang;
                if (ang2 > bestAng2) continue;

                bestAng2       = ang2;
                bestYawDelta   = dy_ang;
                bestPitchDelta = dp_ang;
                haveTarget     = true;
            }

            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
            lc->env->PopLocalFrame(nullptr);

            if (!haveTarget) continue;

            const float fH = (float)g_settings.aimSpeedH / 10.0f;
            const float fV = (float)g_settings.aimSpeedV / 10.0f;
            float stepYaw   = bestYawDelta   * fH * fH * 0.22f;
            float stepPitch = bestPitchDelta * fV * fV * 0.22f;

            constexpr float MAX_STEP_DEG = 8.0f;
            if (stepYaw   >  MAX_STEP_DEG) stepYaw   =  MAX_STEP_DEG;
            if (stepYaw   < -MAX_STEP_DEG) stepYaw   = -MAX_STEP_DEG;
            if (stepPitch >  MAX_STEP_DEG) stepPitch =  MAX_STEP_DEG;
            if (stepPitch < -MAX_STEP_DEG) stepPitch = -MAX_STEP_DEG;

            constexpr float PIXELS_PER_DEGREE = 8.0f;
            const int pxX = (int)std::lround(stepYaw   * PIXELS_PER_DEGREE);
            const int pxY = (int)std::lround(stepPitch * PIXELS_PER_DEGREE);

            injectMouseMove(pxX, pxY);
        }

        AC_LOG("aim: loop exit; detaching");
        lc->vm->DetachCurrentThread();
        return 0;
    }
}
