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

    // stage: 0=no inv, 1=bad sel, 2=empty, 3=item null, 4=class null, 5=not block, 6=block
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

        auto lastLog = std::chrono::steady_clock::now();

        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<double> marginDist(0.1, 0.2);
        double margin    = marginDist(rng);
        bool   prevSneak = false;

        while (!AutoclickerModule::destruct)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            const bool enabled = g_settings.scaffoldEnabled;
            const bool fg      = (GetForegroundWindow() == mcWindow);
            const bool menu    = Overlay::IsMenuVisible();
            const bool sKey    = (GetAsyncKeyState('S') & 0x8000) != 0;

            bool   decided   = false;
            bool   wantSneak = false;
            int    dbgBlock = -1, dbgGround = -1, dbgNbrAir = -1;
            float  dbgPitch = 0.0f;
            double dbgDist = 9.0, dbgX = 0, dbgY = 0, dbgZ = 0;

            if (enabled && fg && !menu && sKey && lc->env->PushLocalFrame(64) == 0)
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
                                dbgPitch = pitch; dbgX = x; dbgY = y; dbgZ = z;

                                decided = true;

                                dbgBlock = blockHeldStage(local);

                                if (pitch > 0.0f && dbgBlock == 6) {
                                    const int by = (int)std::floor(y) - 1;

                                    const double yawR = yaw * M_PI / 180.0;
                                    const double dxb =  std::sin(yawR);
                                    const double dzb = -std::cos(yawR);

                                    constexpr double HALF   = 0.3;

                                    int    rbx, rbz, nbx, nbz;
                                    double distToEdge;
                                    if (std::fabs(dxb) >= std::fabs(dzb)) {
                                        rbz = (int)std::floor(z); nbz = rbz;
                                        if (dxb < 0.0) { rbx = (int)std::floor(x + HALF); distToEdge = (x - HALF) - rbx;       nbx = rbx - 1; }
                                        else           { rbx = (int)std::floor(x - HALF); distToEdge = (rbx + 1) - (x + HALF); nbx = rbx + 1; }
                                    } else {
                                        rbx = (int)std::floor(x); nbx = rbx;
                                        if (dzb < 0.0) { rbz = (int)std::floor(z + HALF); distToEdge = (z - HALF) - rbz;       nbz = rbz - 1; }
                                        else           { rbz = (int)std::floor(z - HALF); distToEdge = (rbz + 1) - (z + HALF); nbz = rbz + 1; }
                                    }
                                    dbgDist = distToEdge;

                                    dbgGround = isAir(level, rbx, by, rbz) ? 0 : 1;
                                    if (dbgGround == 1) {
                                        dbgNbrAir = isAir(level, nbx, by, nbz) ? 1 : 0;
                                        if (distToEdge <= margin && dbgNbrAir == 1)
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

            if (!enabled || menu || !fg || !sKey) {
                setSneak(false);
            }
            else if (decided) {
                setSneak(wantSneak);
                if (prevSneak && !wantSneak) margin = marginDist(rng);
                prevSneak = wantSneak;
            }

            const auto now = std::chrono::steady_clock::now();
            if (enabled &&
                std::chrono::duration_cast<std::chrono::milliseconds>(now - lastLog).count() >= 300) {
                lastLog = now;
                AC_LOG("scaffold: en=%d fg=%d menu=%d S=%d | pitch=%.1f block=%d ground=%d dist=%.3f margin=%.3f nbrAir=%d sneak=%d pos=(%.2f,%.2f,%.2f)",
                       enabled, fg, menu, sKey, dbgPitch, dbgBlock, dbgGround, dbgDist, margin, dbgNbrAir, wantSneak, dbgX, dbgY, dbgZ);
            }
        }

        setSneak(false);
        AC_LOG("scaffold: loop exit; detaching");
        lc->vm->DetachCurrentThread();
        return 0;
    }
}
