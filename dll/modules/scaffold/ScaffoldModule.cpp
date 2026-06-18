#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "ScaffoldModule.h"
#include <Windows.h>
#include "../../config/Settings.h"
#include "../../SDK/Minecraft.h"
#include "Mappings.h"
#include "../../overlay/Overlay.h"
#include <chrono>
#include <cmath>
#include <random>
#include <vector>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ScaffoldModule
{
    static void setSneak(bool down)
    {
        static bool cur = false;
        if (down == cur) return;
        cur = down;

        INPUT in      = {};
        in.type       = INPUT_KEYBOARD;
        in.ki.wScan   = (WORD)MapVirtualKeyW(VK_LSHIFT, MAPVK_VK_TO_VSC);
        in.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0u : KEYEVENTF_KEYUP);
        SendInput(1, &in, sizeof(INPUT));
    }

    static bool isAir(jobject levelInstance, int bx, int by, int bz)
    {
        static jclass    bpCls = nullptr, lvCls = nullptr, bsCls = nullptr;
        static jmethodID bpCtor = nullptr, getBS = nullptr, blocksMotion = nullptr;

        if (getBS == nullptr) {
            bpCls = lc->GetClass(MC_BlockPos);
            lvCls = lc->GetClass(MC_ClientLevel);
            bsCls = lc->GetClass(MC_BlockState);
            if (!bpCls || !lvCls || !bsCls) return false;
            bpCtor       = lc->env->GetMethodID(bpCls, "<init>", "(III)V");
            getBS        = lc->env->GetMethodID(lvCls, MTD_Level_getBlockState, DESC_Level_getBlockState);
            blocksMotion = lc->env->GetMethodID(bsCls, MTD_BlockState_blocksMotion, "()Z");
            if (!bpCtor || !getBS || !blocksMotion) { lc->env->ExceptionClear(); getBS = nullptr; return false; }
        }

        jobject bp = lc->env->NewObject(bpCls, bpCtor, (jint)bx, (jint)by, (jint)bz);
        if (bp == nullptr || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return false; }
        jobject bs = lc->env->CallObjectMethod(levelInstance, getBS, bp);
        lc->env->DeleteLocalRef(bp);
        if (bs == nullptr || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return false; }
        const jboolean solid = lc->env->CallBooleanMethod(bs, blocksMotion);
        lc->env->DeleteLocalRef(bs);
        if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return false; }
        return solid != JNI_TRUE;
    }

    static bool holdingBlock(Player& local)
    {
        Inventory inv = local.getInventory();
        if (inv.GetInstance() == nullptr) return false;
        const int sel = inv.getSelected();
        if (sel < 0 || sel > 8) return false;
        ItemStack stack = inv.getItem(sel);
        if (stack.GetInstance() == nullptr || stack.isEmpty()) return false;
        Item item = stack.getItem();
        if (item.GetInstance() == nullptr) return false;
        jclass cls = lc->GetClass(MC_BlockItem);
        return cls != nullptr &&
               lc->env->IsInstanceOf(item.GetInstance(), cls) == JNI_TRUE;
    }

    static bool anyCornerSolid(jobject lv, AABB& box, int by)
    {
        return !isAir(lv, (int)std::floor(box.minX()), by, (int)std::floor(box.minZ()))
            || !isAir(lv, (int)std::floor(box.maxX()), by, (int)std::floor(box.minZ()))
            || !isAir(lv, (int)std::floor(box.minX()), by, (int)std::floor(box.maxZ()))
            || !isAir(lv, (int)std::floor(box.maxX()), by, (int)std::floor(box.maxZ()));
    }

    static bool shouldShift(jobject lv, AABB& box, int by, double dx, double dz)
    {
        static bool   isLatching = false;
        static double lastNdx    = 0.0;
        static double lastNdz    = 0.0;

        const double dl = std::sqrt(dx * dx + dz * dz);
        double ndx = lastNdx;
        double ndz = lastNdz;

        if (dl > 1e-4) {
            ndx = dx / dl;
            ndz = dz / dl;
            lastNdx = ndx;
            lastNdz = ndz;
        }

        // If we've never moved, we can't shift
        if (std::abs(ndx) < 0.001 && std::abs(ndz) < 0.001) return false;

        const int cx = (int)std::floor((box.minX() + box.maxX()) * 0.5);
        const int cz = (int)std::floor((box.minZ() + box.maxZ()) * 0.5);

        // Projected "target" block in movement direction
        const int nx = (int)std::floor(((box.minX() + box.maxX()) * 0.5) + (ndx * 0.45));
        const int nz = (int)std::floor(((box.minZ() + box.maxZ()) * 0.5) + (ndz * 0.45));

        // Distance to the edge of the current block
        const double boundX = (ndx >= 0) ? (double)cx + 1.0 : (double)cx;
        const double boundZ = (ndz >= 0) ? (double)cz + 1.0 : (double)cz;

        double past = 0;
        if (std::abs(ndx) > 0.01) {
            double leadX = (ndx > 0) ? box.maxX() : box.minX();
            past = (std::max)(past, std::abs(leadX - boundX));
        }
        if (std::abs(ndz) > 0.01) {
            double leadZ = (ndz > 0) ? box.maxZ() : box.minZ();
            past = (std::max)(past, std::abs(leadZ - boundZ));
        }

        if (!isLatching) {
            // Trigger shift at 90% (0.54)
            if (isAir(lv, nx, by, nz) && past > 0.535) {
                isLatching = true;
            }
        } else {
            // Stay latched until:
            // 1. We are safely back on the block (past < 0.15)
            // 2. OR the block in front is now solid (we placed a block)
            if (past < 0.15 || !isAir(lv, nx, by, nz)) {
                isLatching = false;
            }
        }

        return isLatching;
    }

    void Release()
    {
        setSneak(false);
    }

    void Tick()
    {
        if (lc->env == nullptr) {
            if (lc->vm == nullptr || !lc->classesLoaded.load(std::memory_order_acquire)) return;
            JNIEnv* e = nullptr;
            if (lc->vm->GetEnv(reinterpret_cast<void**>(&e), JNI_VERSION_1_6) == JNI_OK && e)
                lc->env = e;
        }
        if (lc->env == nullptr) return;

        static auto last = std::chrono::steady_clock::now();
        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count() < 10)
            return;
        last = now;

        static HWND mcWindow = nullptr;
        if (mcWindow == nullptr) mcWindow = FindWindowW(L"GLFW30", nullptr);

        const bool kW = (GetAsyncKeyState('W') & 0x8000) != 0;
        const bool kA = (GetAsyncKeyState('A') & 0x8000) != 0;
        const bool kS = (GetAsyncKeyState('S') & 0x8000) != 0;
        const bool kD = (GetAsyncKeyState('D') & 0x8000) != 0;
        const bool moveKey = kW || kA || kS || kD;

        if (!g_settings.scaffoldEnabled || Overlay::IsMenuVisible()
            || GetForegroundWindow() != mcWindow || kW) {
            setSneak(false);
            return;
        }

        bool decided   = false;
        bool wantSneak = false;

        if (lc->env->PushLocalFrame(64) == 0) {
            Minecraft mc;
            if (mc.isPaused()) {
                decided = true;
            }
            else {
                if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                Screen screen = mc.GetScreen();
                if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
                if (screen.GetInstance() != nullptr) {
                    decided = true;
                }
                else {
                    Player local = mc.GetLocalPlayer();
                    Level  level = mc.GetLevel();
                    if (local.GetInstance() != nullptr && level.GetInstance() != nullptr) {
                        AABB box = local.getBoundingBox();
                        if (box.GetInstance() != nullptr) {
                            const double x   = local.getX();
                            const double y   = local.getY();
                            const double z   = local.getZ();
                            const double xo  = local.getXo();
                            const double zo  = local.getZo();
                            const float  yaw = local.getYRot();

                            decided = true;

                            if (holdingBlock(local)) {
                                const int     by = (int)std::floor(y) - 1;
                                const jobject lv = level.GetInstance();

                                if (anyCornerSolid(lv, box, by)) {
                                    double dx = x - xo;
                                    double dz = z - zo;

                                    if (std::sqrt(dx * dx + dz * dz) < 1e-4 && moveKey) {
                                        const double yawR = yaw * M_PI / 180.0;
                                        const double fwdX = -std::sin(yawR), fwdZ =  std::cos(yawR);
                                        const double rgtX = -std::cos(yawR), rgtZ = -std::sin(yawR);
                                        if (kW) { dx += fwdX; dz += fwdZ; }
                                        if (kS) { dx -= fwdX; dz -= fwdZ; }
                                        if (kD) { dx += rgtX; dz += rgtZ; }
                                        if (kA) { dx -= rgtX; dz -= rgtZ; }
                                    }

                                    if (shouldShift(lv, box, by, dx, dz))
                                        wantSneak = true;
                                }
                            }
                        }
                    }
                }
            }
            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
            lc->env->PopLocalFrame(nullptr);
        }

        if (decided) {
            setSneak(wantSneak);
        }
    }
}
