#include "AimAssistModule.h"
#include "../../Settings.h"
#include "../../SDK/Minecraft.h"
#include "../../SDK/BlockPos.h"
#include "../../SDK/BlockState.h"
#include "../autoclicker/AutoclickerModule.h"
#include "../../overlay/Overlay.h"
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

    // Stepped block raycast from (sx,sy,sz) to (tx,ty,tz). Returns false the
    // moment any sampled BlockPos along the ray has blocksMotion=true. We don't
    // use MC's own Level.clip(ClipContext) because constructing ClipContext
    // requires its inner Block/Fluid enums + Entity arg — significant JNI
    // dance for the same answer this stepped check gives.
    //
    // Step size 0.30 blocks: small enough to catch a 1-thick wall reliably
    // (any block whose face the ray crosses is sampled at least once) without
    // exploding the JNI count. startOffset 0.30 skips the eye block (our own
    // head is air but adjacent walls glued to us aren't worth false-negating
    // on); endOffset 0.40 backs off short of the target body so we don't
    // false-positive on the block their feet/torso occupies.
    //
    // Caller MUST PushLocalFrame around the call — each step allocates 2
    // refs (BlockPos + BlockState), and a long ray could push tens of refs.
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
            // Skip if we landed in the same block we just tested — saves
            // a JNI roundtrip when the ray is shallow / nearly axis-aligned.
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

    // Team colour read the EXACT same way ESP colours its boxes: the last
    // non-empty chunk of the team-formatted name carries the team colour.
    // Returns white (0xFFFFFFFF) when there's no name / no team colour. This is
    // the same crash-free path ESP runs every frame — reused here so aim's
    // teammate test agrees with what the boxes show.
    static uint32_t teamColorOf(Entity& e)
    {
        auto chunks = e.getFormattedNameChunks();
        for (auto it = chunks.rbegin(); it != chunks.rend(); ++it)
            if (!it->first.empty()) return it->second;
        return 0xFFFFFFFFu;
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
            // position refs don't leak across ticks. 256 to match ESP, which
            // does the same per-player name read every frame without issue.
            if (lc->env->PushLocalFrame(256) != 0) {
                lc->env->ExceptionClear();
                continue;
            }

            // MC menu gate. Any non-null Minecraft.screen means a menu is up
            // (chat, inventory, pause, ESC menu, GUI containers, etc.) — and
            // isPaused() covers the rest (connection-lost, internal pause).
            // Don't nudge the cursor while the user is interacting with a
            // menu; the assist would fight whatever click/drag they're doing.
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

            // Eye position. 1.62 is the standing eye height; sneak/swim alter
            // it (1.27 / 0.4 respectively) but we don't bother — the assist is
            // a closed loop and self-corrects on the next tick if the eye Y
            // estimate is slightly off.
            const double ex       = lpos.getX();
            const double ey       = lpos.getY() + 1.62;
            const double ez       = lpos.getZ();
            const float  curYaw   = local.getYRot();
            const float  curPitch = local.getXRot();

            // Local player's team colour, read the exact same way ESP colours
            // its boxes. Gated by the "Teams by Colour" toggle (on = colour
            // boxes AND skip teammates); white means "no team colour" → don't
            // treat anyone as a teammate.
            const uint32_t myTeamColor =
                g_settings.teamsByColor ? teamColorOf(local) : 0xFFFFFFFFu;

            auto players = level.players();
            const jobject localInst = local.GetInstance();
            const double  maxDistSq = (double)g_settings.aimRange * (double)g_settings.aimRange;
            const float   halfFov   = g_settings.aimFov * 0.5f;

            // Pick the target by smallest angular distance to crosshair within
            // the FOV cone. The aim point isn't a fixed body offset — it's
            // the closest point on the target's AABB to the current view
            // ray, computed by projecting the 8 AABB corners into angular
            // space and clamping the cursor's (yaw, pitch) to that region.
            // When the cursor is already inside that region (i.e. the ray
            // pierces the hitbox), both deltas are 0 — assist stops nudging.
            float bestAng2       = halfFov * halfFov;
            float bestYawDelta   = 0.f;
            float bestPitchDelta = 0.f;
            bool  haveTarget     = false;

            for (auto& p : players)
            {
                if (lc->env->IsSameObject(p.GetInstance(), localInst)) continue;

                // Friend skip. Mirrors the team-skip policy: a player you've
                // marked as a friend should never be a valid aim target,
                // regardless of what scoreboard team the server has them on.
                // Cheap to compute — getName().getString() is one JNI call
                // and the friends list is small. Skip the whole block when
                // the list is empty so cross-server play with no friends
                // configured doesn't pay even the linear-scan cost.
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

                // Same-colour (same team) skip — done after the cheap distance
                // gate so the per-player name read (the exact one ESP uses) is
                // only paid for in-range players, not the whole server. White =
                // no team colour = attack everyone.
                if (myTeamColor != 0xFFFFFFFFu && teamColorOf(p) == myTeamColor)
                    continue;

                AABB box = p.getBoundingBox();
                if (box.GetInstance() == nullptr) continue;
                const double minX = box.minX(), maxX = box.maxX();
                const double minY = box.minY(), maxY = box.maxY();
                const double minZ = box.minZ(), maxZ = box.maxZ();

                // Line-of-sight gate. We don't want to aim through walls — so
                // sample three points spaced down the body (eye / chest / feet)
                // and require at least one to be reachable. Single-point LoS
                // false-negatives on tall walls where only the head pokes out;
                // requiring all-three false-negatives on a fence post crossing
                // the chest. "Any one of three" matches what a player can
                // actually hit. Each raycast goes inside its own local frame
                // so the per-step BlockPos / BlockState refs don't accumulate
                // into the outer player-iter frame (which is sized for ~6
                // players, not 6 × ~20 ray steps × 2 refs each).
                {
                    const double cxBody = (minX + maxX) * 0.5;
                    const double czBody = (minZ + maxZ) * 0.5;
                    const double bodyY  = (minY + maxY) * 0.5;
                    const double eyeY   = maxY - 0.18;      // ~just below top of head
                    const double feetY  = minY + 0.20;      // just above feet
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

                // Shrink the AABB to its inner core before projecting.
                // The assist clamps the cursor into whatever box we feed
                // it — if we feed the full hitbox, the assist stops the
                // moment the ray crosses the body's outer edge, leaving
                // the cursor on the rim where small flicks can miss.
                // Shrinking to 55% keeps the "valid target zone" inside
                // the body, so when the cursor settles it's well-centered.
                // Not 50% so a player who's already aimed at the body
                // doesn't get yanked harder toward the geometric center
                // every tick.
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

                // Walk the 8 (inner) AABB corners, build the angular
                // bounding box around the cursor. Yaw is unwrapped to be
                // near curYaw so the ±180 seam can't make the min/max
                // collapse.
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

                // Clamp cursor angles into the box. Inside → delta 0 → stop.
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

            // Per-tick step toward target. Quadratic so the low end of the
            // 0-10 slider feels gentle (1 → ~0.2% of remaining per tick)
            // and the top is snappy (10 → 22% of remaining per tick).
            // Coefficient bumped from 0.10 → 0.22 — old top-end felt weak
            // (half-life ~70ms at slider=10); new value gives half-life
            // ~28ms which lands clearly on the target without overshoot.
            const float fH = (float)g_settings.aimSpeedH / 10.0f;
            const float fV = (float)g_settings.aimSpeedV / 10.0f;
            float stepYaw   = bestYawDelta   * fH * fH * 0.22f;
            float stepPitch = bestPitchDelta * fV * fV * 0.22f;

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
