#include "LeapModule.h"
#include "../../Settings.h"
#include "../../SDK/Minecraft.h"
#include "../autoclicker/AutoclickerModule.h"
#include "../../overlay/Overlay.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <string>
#include <thread>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Auto-leap. Mirrors the server's Champions/axe/Leap wallKick() logic
// client-side so we only fire the right-click when conditions actually
// hold — back-against-solid-block + air-ahead + facing in the correct
// quadrant — and otherwise stay silent. Cooldown-only branch of the
// skill (the wall-kick path) has no skill cooldown server-side; just an
// internal 0.5s wall-kick cooldown + energy cost. Pacing at >=500ms
// avoids wasting energy on rejected presses.
namespace LeapModule
{
    static bool jvmReady()
    {
        return lc->vm != nullptr && lc->classesLoaded.load(std::memory_order_acquire);
    }

    static bool ContainsCi(const std::string& hay, const char* needle)
    {
        if (!needle || !*needle) return false;
        std::string h = hay;
        std::string n = needle;
        std::transform(h.begin(), h.end(), h.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });
        std::transform(n.begin(), n.end(), n.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });
        return h.find(n) != std::string::npos;
    }

    // Display name of the selected hotbar slot. Empty on any JNI failure or
    // empty slot — caller treats as "not an axe" so suppression is the safe
    // default (don't fire blindly with the wrong item in hand).
    static std::string SelectedItemName(Minecraft& mc)
    {
        if (lc->env->PushLocalFrame(32) != 0) {
            lc->env->ExceptionClear();
            return {};
        }
        std::string out;
        Player player = mc.GetLocalPlayer();
        if (player.GetInstance() != nullptr) {
            Inventory inv = player.getInventory();
            if (inv.GetInstance() != nullptr) {
                const int sel = inv.getSelected();
                if (sel >= 0 && sel <= 8) {
                    ItemStack stack = inv.getItem(sel);
                    if (stack.GetInstance() != nullptr && !stack.isEmpty()) {
                        Component name = stack.getHoverName();
                        if (name.GetInstance() != nullptr)
                            out = name.getString();
                    }
                }
            }
        }
        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        lc->env->PopLocalFrame(nullptr);
        return out;
    }

    // Synthetic right-click into MC's window. 30ms hold mirrors macros/AC
    // rclick — short enough to feel like one press, long enough that MC's
    // tick reliably sees the down→up sequence.
    static void RightClick(HWND hwnd)
    {
        POINT pt;
        GetCursorPos(&pt);
        const LPARAM coord = MAKELPARAM(pt.x, pt.y);
        SendMessageW(hwnd, WM_RBUTTONDOWN, MK_RBUTTON, coord);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        SendMessageW(hwnd, WM_RBUTTONUP,   MK_RBUTTON, coord);
    }

    // Client-side replica of the server's wallKick precondition check.
    // Returns true when right-clicking would fire the cooldown-free
    // wall-kick branch on the server. Reads only the 4-5 block cells we
    // care about, so the JNI cost is fixed regardless of view distance.
    //
    // Server logic (Leap.java):
    //   facing  = (-sin(yaw), 0, cos(yaw))    // y component irrelevant
    //   xPos    = facing.x >= 0,  zPos = facing.z >= 0
    //   For each (dx, dz) in {-1,0,1}^2 \ {(0,0)}:
    //     gated to the back quadrant by:
    //       (!xPos || dx<=0) && (!zPos || dz<=0) &&
    //       ( xPos || dx>=0) && ( zPos || dz>=0)
    //     if block at (floor(px)+dx, floor(py), floor(pz)+dz) is solid:
    //       wall present
    //   forward block (one up + one ahead):
    //     if |facing.x| > |facing.z|: (sign(facing.x), 1, 0)
    //     else                       : (0, 1, sign(facing.z))
    //     forward must be air
    //
    // Back-wall check uses blocksMotion() — true only for blocks whose
    // collision box stops entity motion. Excludes air AND foliage / fluids
    // / decorations, matching the server's airFoliage exclusion. !isAir
    // alone counted tall grass as a wall and caused false-positive fires.
    // Forward-air check stays on isAir — we want any non-collidable cell
    // ahead so the kick velocity isn't immediately consumed by an obstacle.
    static bool WallKickConditionsMet(Minecraft& mc)
    {
        bool result = false;

        if (lc->env->PushLocalFrame(32) != 0) {
            lc->env->ExceptionClear();
            return false;
        }

        Player local = mc.GetLocalPlayer();
        if (local.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); return false; }

        Level level = mc.GetLevel();
        if (level.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); return false; }

        Vec3 pos = local.getPosition();
        if (pos.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); return false; }

        const double px = pos.getX();
        const double py = pos.getY();
        const double pz = pos.getZ();
        const float  yawDeg = local.getYRot();
        const double yawRad = (double)yawDeg * M_PI / 180.0;

        // MC yaw convention (verified against AimAssist): yaw=0 → +Z,
        // counter-clockwise around +Y → yaw=90 → -X.
        const double fx = -std::sin(yawRad);
        const double fz =  std::cos(yawRad);
        const bool xPos = fx >= 0.0;
        const bool zPos = fz >= 0.0;

        // floor() not (int) — for negative coordinates (int)px truncates
        // toward zero, but the block containing -0.5 is at block-y = -1,
        // not 0. floor() gets the block-coord even on the south/west side
        // of the origin.
        const int bx = (int)std::floor(px);
        const int by = (int)std::floor(py);
        const int bz = (int)std::floor(pz);

        bool wallFound = false;
        for (int dx = -1; dx <= 1 && !wallFound; ++dx) {
            for (int dz = -1; dz <= 1 && !wallFound; ++dz) {
                if (dx == 0 && dz == 0) continue;
                // Back-quadrant gate (verbatim from server).
                if (!((!xPos || dx <= 0) && (!zPos || dz <= 0) &&
                      ( xPos || dx >= 0) && ( zPos || dz >= 0)))
                    continue;

                BlockPos bp = BlockPos::make(bx + dx, by, bz + dz);
                if (bp.GetInstance() == nullptr) continue;
                BlockState st = level.getBlockState(bp);
                lc->env->DeleteLocalRef(bp.GetInstance());

                if (st.GetInstance() == nullptr) continue;
                const bool solid = st.blocksMotion();
                lc->env->DeleteLocalRef(st.GetInstance());

                if (solid) wallFound = true;
            }
        }

        if (wallFound) {
            // Forward block: one ahead + one up. Picks X or Z axis based on
            // which component of facing is larger (mirrors the server's
            // getForwardBlock).
            int fwdX, fwdZ;
            if (std::abs(fx) > std::abs(fz)) {
                fwdX = (fx >= 0.0) ? 1 : -1;
                fwdZ = 0;
            } else {
                fwdX = 0;
                fwdZ = (fz >= 0.0) ? 1 : -1;
            }
            BlockPos fp = BlockPos::make(bx + fwdX, by + 1, bz + fwdZ);
            if (fp.GetInstance() != nullptr) {
                BlockState fs = level.getBlockState(fp);
                lc->env->DeleteLocalRef(fp.GetInstance());
                if (fs.GetInstance() != nullptr) {
                    if (fs.isAir()) result = true;
                    lc->env->DeleteLocalRef(fs.GetInstance());
                }
            }
        }

        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        lc->env->PopLocalFrame(nullptr);
        return result;
    }

    DWORD WINAPI init(LPVOID /*lpParam*/)
    {
        // Wait for AC to attach + populate the class map.
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

        // Edge-trigger state. Fire once on the rising edge of
        // WallKickConditionsMet, then stay silent until the conditions
        // drop (player flies away from the wall on the kick, lands
        // elsewhere, walks back). Without this, sitting against a wall
        // with the module enabled would auto-fire every `leapInterval`
        // ms — exactly the "spam" pattern we want to avoid.
        bool prevConditions = false;

        // Hardware-safety floor against pathological condition-state
        // flapping (player skirting the exact block boundary, etc.).
        // Edge detection handles the common case; this is the lower
        // bound between any two fires.
        auto lastClick = std::chrono::steady_clock::now()
                       - std::chrono::seconds(10);

        while (!AutoclickerModule::destruct)
        {
            // 20Hz — matches MC's tick rate, so a single tick scan can
            // observe every game-state change once. Block-state reads on
            // the local chunk are cheap (no chunk fetch, just a hash lookup
            // into LevelChunk + section array), so 20Hz costs nothing.
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            // Any gate failure resets edge tracking. Otherwise the user
            // disabling + re-enabling (or alt-tabbing out and back) while
            // already against a wall would skip the first valid fire —
            // prevConditions would still read true from before the gate
            // flipped, suppressing the next rising-edge detection.
            if (!g_settings.leapEnabled)              { prevConditions = false; continue; }
            if (Overlay::IsMenuVisible())             { prevConditions = false; continue; }
            if (GetForegroundWindow() != mcWindow)    { prevConditions = false; continue; }

            if (g_settings.leapKey > 0 && g_settings.leapKey <= 0xFE) {
                if (!(GetAsyncKeyState(g_settings.leapKey) & 0x8000)) {
                    prevConditions = false;
                    continue;
                }
            }

            if (g_settings.leapRequireAxe) {
                const std::string name = SelectedItemName(mc);
                if (!ContainsCi(name, "axe")) { prevConditions = false; continue; }
            }

            const bool cur  = WallKickConditionsMet(mc);
            const bool edge = cur && !prevConditions;
            prevConditions  = cur;

            if (!edge) continue;

            const auto now = std::chrono::steady_clock::now();
            const auto sinceLast = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastClick).count();
            if (sinceLast < g_settings.leapInterval) continue;

            RightClick(mcWindow);
            lastClick = std::chrono::steady_clock::now();
        }

        lc->vm->DetachCurrentThread();
        return 0;
    }
}
