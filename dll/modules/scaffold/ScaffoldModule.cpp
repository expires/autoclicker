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

        while (!AutoclickerModule::destruct)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            if (!g_settings.scaffoldEnabled
                || Overlay::IsMenuVisible()
                || GetForegroundWindow() != mcWindow)
            {
                setSneak(false);
                continue;
            }

            if (!(GetAsyncKeyState('S') & 0x8000)) { setSneak(false); continue; }

            bool decided   = false;
            bool wantSneak = false;

            if (lc->env->PushLocalFrame(64) == 0)
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

                                if (pitch > 0.0f && holdingBlock(local)) {
                                    const int bx = (int)std::floor(x);
                                    const int bz = (int)std::floor(z);
                                    const int by = (int)std::floor(y) - 1;

                                    if (!isAir(level, bx, by, bz)) {
                                        const double yawR = yaw * M_PI / 180.0;
                                        const double dxb =  std::sin(yawR);
                                        const double dzb = -std::cos(yawR);

                                        constexpr double HALF   = 0.3;
                                        constexpr double MARGIN = 0.15;
                                        double distToEdge;
                                        int nbx = bx, nbz = bz;
                                        if (std::fabs(dxb) >= std::fabs(dzb)) {
                                            if (dxb < 0.0) { distToEdge = (x - HALF) - bx;       nbx = bx - 1; }
                                            else           { distToEdge = (bx + 1) - (x + HALF); nbx = bx + 1; }
                                        } else {
                                            if (dzb < 0.0) { distToEdge = (z - HALF) - bz;       nbz = bz - 1; }
                                            else           { distToEdge = (bz + 1) - (z + HALF); nbz = bz + 1; }
                                        }

                                        if (distToEdge <= MARGIN && isAir(level, nbx, by, nbz))
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

            if (decided) setSneak(wantSneak);
        }

        setSneak(false);
        AC_LOG("scaffold: loop exit; detaching");
        lc->vm->DetachCurrentThread();
        return 0;
    }
}
