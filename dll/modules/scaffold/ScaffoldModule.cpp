#include "ScaffoldModule.h"
#include <Windows.h>
#include "../../config/Settings.h"
#include "../../SDK/Minecraft.h"
#include "Mappings.h"
#include "../../overlay/Overlay.h"
#include <chrono>
#include <cmath>
#include <random>

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

    // Resolves the class/method ids once and reuses them, and calls getBlockState
    // directly on the level instance (no per-read GetClass mutex / GetMethodID).
    // Render-thread only, so the statics are single-threaded.
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

    static bool isCloseToEdge(jobject lv, AABB& box, int by,
                              double dx, double dz, double edge)
    {
        const double dl = std::sqrt(dx * dx + dz * dz);
        if (dl <= 1e-3) return false;
        const double ndx = dx / dl;
        const double ndz = dz / dl;

        const double startX = (ndx > 0) ? box.maxX() : (ndx < 0 ? box.minX() : (box.minX() + box.maxX()) / 2.0);
        const double startZ = (ndz > 0) ? box.maxZ() : (ndz < 0 ? box.minZ() : (box.minZ() + box.maxZ()) / 2.0);

        constexpr int STEPS = 6;
        for (int i = 1; i <= STEPS; ++i) {
            const double t = edge * (double)i / STEPS;
            if (isAir(lv, (int)std::floor(startX + ndx * t), by, (int)std::floor(startZ + ndz * t)))
                return true;
        }
        return false;
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

        static std::mt19937 rng(std::random_device{}());
        static std::uniform_real_distribution<double> edgeRange(0.05, 0.15);
        static double edge      = edgeRange(rng);
        static bool   prevSneak = false;

        const bool kW = (GetAsyncKeyState('W') & 0x8000) != 0;
        const bool kA = (GetAsyncKeyState('A') & 0x8000) != 0;
        const bool kS = (GetAsyncKeyState('S') & 0x8000) != 0;
        const bool kD = (GetAsyncKeyState('D') & 0x8000) != 0;
        const bool moveKey = kW || kA || kS || kD;

        if (!g_settings.scaffoldEnabled || Overlay::IsMenuVisible()
            || GetForegroundWindow() != mcWindow) {
            setSneak(false);
            prevSneak = false;
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
                            const float  yaw = local.getYRot();

                            decided = true;

                            if (holdingBlock(local)) {
                                const int     by = (int)std::floor(y) - 1;
                                const jobject lv = level.GetInstance();

                                if (anyCornerSolid(lv, box, by)) {
                                    const double yawR = yaw * M_PI / 180.0;
                                    const double fwdX = -std::sin(yawR), fwdZ =  std::cos(yawR);
                                    const double rgtX = -std::cos(yawR), rgtZ = -std::sin(yawR);

                                    double dx = 0.0, dz = 0.0;
                                    if (kW) { dx += fwdX; dz += fwdZ; }
                                    if (kS) { dx -= fwdX; dz -= fwdZ; }
                                    if (kD) { dx += rgtX; dz += rgtZ; }
                                    if (kA) { dx -= rgtX; dz -= rgtZ; }

                                    if (moveKey) {
                                        if (isCloseToEdge(lv, box, by, dx, dz, edge))
                                            wantSneak = true;
                                    }
                                    else {
                                        // Stationary check: stay sneaking if any part of the hitbox is over air.
                                        if (isAir(lv, (int)std::floor(box.minX()), by, (int)std::floor(box.minZ()))
                                            || isAir(lv, (int)std::floor(box.maxX()), by, (int)std::floor(box.minZ()))
                                            || isAir(lv, (int)std::floor(box.minX()), by, (int)std::floor(box.maxZ()))
                                            || isAir(lv, (int)std::floor(box.maxX()), by, (int)std::floor(box.maxZ())))
                                        {
                                            wantSneak = true;
                                        }
                                    }
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
            if (prevSneak && !wantSneak) edge = edgeRange(rng);
            prevSneak = wantSneak;
        }
    }
}
