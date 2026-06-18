#include "ScaffoldModule.h"
#include "../../config/Settings.h"
#include "../../SDK/Minecraft.h"
#include "../../SDK/BlockPos.h"
#include "../../SDK/BlockState.h"
#include "Mappings.h"
#include "../autoclicker/AutoclickerModule.h"
#include "../../overlay/Overlay.h"
#include "../../logger/Logger.h"
#include <chrono>
#include <cmath>
#include <random>
#include <thread>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ScaffoldModule
{
    static bool jvmReady()
    {
        return lc->vm != nullptr && lc->classesLoaded.load(std::memory_order_acquire);
    }

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

    static bool isAir(Level& level, int bx, int by, int bz)
    {
        BlockPos bp = BlockPos::make(bx, by, bz);
        if (!bp.GetInstance()) return false;
        BlockState bs = level.getBlockState(bp);
        return !bs.blocksMotion();
    }

    static int blockHeldStage(Player& local)
    {
        Inventory inv = local.getInventory();
        if (inv.GetInstance() == nullptr) return 0;

        const int sel = inv.getSelected();
        if (sel < 0 || sel > 8) return 1;

        ItemStack stack = inv.getItem(sel);
        if (stack.GetInstance() == nullptr || stack.isEmpty()) return 2;

        Item item = stack.getItem();
        if (item.GetInstance() == nullptr) return 3;

        jclass cls = lc->GetClass(MC_BlockItem);
        if (cls == nullptr) return 4;

        return (lc->env->IsInstanceOf(item.GetInstance(), cls) == JNI_TRUE) ? 6 : 5;
    }

    DWORD WINAPI init(LPVOID)
    {
        AC_LOG("scaffold: thread start");
        while (!AutoclickerModule::destruct && !jvmReady())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (AutoclickerModule::destruct) return 0;

        if (lc->vm->AttachCurrentThread(reinterpret_cast<void**>(&lc->env), nullptr) != JNI_OK)
            return 0;
        if (lc->env == nullptr) return 0;
        AC_LOG("scaffold: attached; entering loop");

        Minecraft  mc;
        const HWND mcWindow = FindWindowW(L"GLFW30", nullptr);
        if (mcWindow == nullptr) {
            lc->vm->DetachCurrentThread();
            return 0;
        }

        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<double> marginDist(0.05, 0.1);
        double margin    = marginDist(rng);
        bool   prevSneak = false;

        while (!AutoclickerModule::destruct)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            const bool enabled = g_settings.scaffoldEnabled;
            const bool fg      = (GetForegroundWindow() == mcWindow);
            const bool menu    = Overlay::IsMenuVisible();
            const bool kW = (GetAsyncKeyState('W') & 0x8000) != 0;
            const bool kA = (GetAsyncKeyState('A') & 0x8000) != 0;
            const bool kS = (GetAsyncKeyState('S') & 0x8000) != 0;
            const bool kD = (GetAsyncKeyState('D') & 0x8000) != 0;
            const bool moveKey = kA || kS || kD;

            bool decided   = false;
            bool wantSneak = false;

            if (enabled && fg && !menu && moveKey && lc->env->PushLocalFrame(64) == 0)
            {
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
                            Vec3 pos = local.getPosition();
                            if (pos.GetInstance() != nullptr) {
                                const double x     = pos.getX();
                                const double y     = pos.getY();
                                const double z     = pos.getZ();
                                const float  yaw   = local.getYRot();
                                const float  pitch = local.getXRot();

                                decided = true;

                                if (pitch > 30.0f && blockHeldStage(local) == 6) {
                                    const double yawR = yaw * M_PI / 180.0;
                                    const double fwdX = -std::sin(yawR), fwdZ =  std::cos(yawR);
                                    const double rgtX = -std::cos(yawR), rgtZ = -std::sin(yawR);

                                    double mdx = 0.0, mdz = 0.0;
                                    if (kW) { mdx += fwdX; mdz += fwdZ; }
                                    if (kS) { mdx -= fwdX; mdz -= fwdZ; }
                                    if (kD) { mdx += rgtX; mdz += rgtZ; }
                                    if (kA) { mdx -= rgtX; mdz -= rgtZ; }

                                    const double mlen = std::sqrt(mdx * mdx + mdz * mdz);
                                    if (mlen > 1e-3) {
                                        mdx /= mlen; mdz /= mlen;

                                        const int by = (int)std::floor(y) - 1;
                                        constexpr double HALF = 0.3;

                                        const int sgnX = (mdx >  0.01) ? 1 : ((mdx < -0.01) ? -1 : 0);
                                        const int sgnZ = (mdz >  0.01) ? 1 : ((mdz < -0.01) ? -1 : 0);

                                        const int refX = (sgnX != 0) ? (int)std::floor(x - sgnX * HALF) : (int)std::floor(x);
                                        const int refZ = (sgnZ != 0) ? (int)std::floor(z - sgnZ * HALF) : (int)std::floor(z);

                                        if (!isAir(level, refX, by, refZ)) {
                                            bool edge = false;

                                            if (sgnX != 0) {
                                                const double distX = (sgnX < 0) ? ((x - HALF) - refX)
                                                                                : ((refX + 1) - (x + HALF));
                                                if (distX <= margin && isAir(level, refX + sgnX, by, refZ))
                                                    edge = true;
                                            }
                                            if (sgnZ != 0) {
                                                const double distZ = (sgnZ < 0) ? ((z - HALF) - refZ)
                                                                                : ((refZ + 1) - (z + HALF));
                                                if (distZ <= margin && isAir(level, refX, by, refZ + sgnZ))
                                                    edge = true;
                                            }

                                            wantSneak = edge;
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

            if (!enabled || menu || !fg || !moveKey) {
                setSneak(false);
            }
            else if (decided) {
                setSneak(wantSneak);
                if (prevSneak && !wantSneak) margin = marginDist(rng);
                prevSneak = wantSneak;
            }
        }

        setSneak(false);
        AC_LOG("scaffold: loop exit; detaching");
        lc->vm->DetachCurrentThread();
        return 0;
    }
}
