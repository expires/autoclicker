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

    static bool anyCornerSolid(Level& level, double x, double z, int by)
    {
        constexpr double H = 0.3;
        return !isAir(level, (int)std::floor(x - H), by, (int)std::floor(z - H))
            || !isAir(level, (int)std::floor(x + H), by, (int)std::floor(z - H))
            || !isAir(level, (int)std::floor(x - H), by, (int)std::floor(z + H))
            || !isAir(level, (int)std::floor(x + H), by, (int)std::floor(z + H));
    }

    static bool isCloseToEdge(Level& level, double x, double z, int by,
                              double dx, double dz, double edge)
    {
        const double dl = std::sqrt(dx * dx + dz * dz);
        if (dl <= 1e-3)
            return false;
        dx /= dl; dz /= dl;

        constexpr int STEPS = 6;
        for (int i = 1; i <= STEPS; ++i) {
            const double t = edge * (double)i / STEPS;
            if (isAir(level, (int)std::floor(x + dx * t), by, (int)std::floor(z + dz * t)))
                return true;
        }
        return false;
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
        std::uniform_real_distribution<double> edgeRange(0.4, 0.6);
        double edge      = edgeRange(rng);
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
            const bool moveKey = kW || kA || kS || kD;

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
                                const double x   = pos.getX();
                                const double y   = pos.getY();
                                const double z   = pos.getZ();
                                const float  yaw = local.getYRot();

                                decided = true;

                                if (holdingBlock(local)) {
                                    const int by = (int)std::floor(y) - 1;

                                    if (anyCornerSolid(level, x, z, by)) {
                                        const double yawR = yaw * M_PI / 180.0;
                                        const double fwdX = -std::sin(yawR), fwdZ =  std::cos(yawR);
                                        const double rgtX = -std::cos(yawR), rgtZ = -std::sin(yawR);

                                        double dx = 0.0, dz = 0.0;
                                        if (kW) { dx += fwdX; dz += fwdZ; }
                                        if (kS) { dx -= fwdX; dz -= fwdZ; }
                                        if (kD) { dx += rgtX; dz += rgtZ; }
                                        if (kA) { dx -= rgtX; dz -= rgtZ; }

                                        if (isCloseToEdge(level, x, z, by, dx, dz, edge))
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

            if (!enabled || menu || !fg || !moveKey) {
                setSneak(false);
            }
            else if (decided) {
                setSneak(wantSneak);
                if (prevSneak && !wantSneak) edge = edgeRange(rng);
                prevSneak = wantSneak;
            }
        }

        setSneak(false);
        AC_LOG("scaffold: loop exit; detaching");
        lc->vm->DetachCurrentThread();
        return 0;
    }
}
