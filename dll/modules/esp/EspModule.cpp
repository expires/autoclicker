#include "EspModule.h"
#include "../../Settings.h"
#include "../../SDK/Minecraft.h"
#include "Mappings.h"
#include "../autoclicker/AutoclickerModule.h"
#include <cctype>
#include <thread>
#include <chrono>

namespace EspModule
{
    std::mutex snapMutex;
    Snapshot   snapshot;

    static bool jvmReady()
    {
        return lc->vm != nullptr && lc->classesLoaded.load(std::memory_order_acquire);
    }

    DWORD WINAPI init(LPVOID lpParam)
    {
        // Wait for the autoclicker thread to attach + populate the class map.
        while (!AutoclickerModule::destruct && !jvmReady())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (AutoclickerModule::destruct) return 0;

        // Attach this thread to the JVM. lc->env is thread_local, so this gives
        // *us* an env without disturbing the autoclicker thread's.
        if (lc->vm->AttachCurrentThread(reinterpret_cast<void**>(&lc->env), nullptr) != JNI_OK)
            return 0;
        if (lc->env == nullptr) return 0;

        Minecraft mc;
        Snapshot back;

        while (!AutoclickerModule::destruct)
        {
            if (!g_settings.espEnabled)
            {
                {
                    std::lock_guard<std::mutex> lk(snapMutex);
                    snapshot.valid = false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            // PushLocalFrame bounds the local refs we create this iteration so
            // they all die at PopLocalFrame — no slow leak across iterations.
            if (lc->env->PushLocalFrame(256) != 0)
            {
                lc->env->ExceptionClear();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            back.targets.clear();
            back.valid           = false;
            back.rawPlayerCount  = -1;
            back.gotMinecraft    = false;
            back.gotLocalPlayer  = false;
            back.gotLevel        = false;
            back.gotGameRenderer = false;
            back.gotCamera       = false;

            auto publishDiag = [&]() {
                std::lock_guard<std::mutex> lk(snapMutex);
                std::swap(snapshot, back);
            };

            jobject mcInst = mc.GetInstance();
            if (mcInst == nullptr) { lc->env->PopLocalFrame(nullptr); publishDiag(); std::this_thread::yield(); continue; }
            back.gotMinecraft = true;

            Player localPlayer = mc.GetLocalPlayer();
            if (localPlayer.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); publishDiag(); std::this_thread::yield(); continue; }
            back.gotLocalPlayer = true;

            Level level = mc.GetLevel();
            if (level.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); publishDiag(); std::this_thread::yield(); continue; }
            back.gotLevel = true;

            GameRenderer gr = mc.GetGameRenderer();
            if (gr.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); publishDiag(); std::this_thread::yield(); continue; }
            back.gotGameRenderer = true;

            Camera cam = gr.getMainCamera();
            if (cam.GetInstance() == nullptr) { lc->env->PopLocalFrame(nullptr); publishDiag(); std::this_thread::yield(); continue; }
            back.gotCamera = true;

            // Partial tick — the render-time interpolation factor MC uses for
            // entity positions and the camera. We read entity positions at the
            // tick boundary (xo / current); the overlay lerps using this value.
            DeltaTracker dt = mc.GetDeltaTracker();
            if (dt.GetInstance() != nullptr)
                back.partialTick = dt.getPartialTick(true);
            else
                back.partialTick = 1.0f;

            // Camera snapshot. Camera.position is already interpolated to the
            // current render's partial tick by Camera.setup(), so we use it
            // as-is without further lerping.
            Vec3 camPos = cam.getPosition();
            back.cam.x    = camPos.getX();
            back.cam.y    = camPos.getY();
            back.cam.z    = camPos.getZ();
            back.cam.yRot = cam.getYRot();
            back.cam.xRot = cam.getXRot();
            back.cam.fov  = gr.getFov(cam, back.partialTick, true);
            if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); back.cam.fov = 70.0f; }

            // Players
            auto players = level.players();
            back.rawPlayerCount = (int)players.size();
            jobject localInst = localPlayer.GetInstance();
            const double maxDistSq =
                (double)g_settings.maxDistance * (double)g_settings.maxDistance;

            // Glow state edge tracking. When the user toggles drawGlow off,
            // the next iteration runs a one-shot pass writing false to every
            // non-local player to actively clear stale glow tags — without
            // this, the last "true" we wrote sticks on every entity until
            // they despawn.
            static bool s_glowWasOn = false;
            const bool glowNow = g_settings.drawGlow;

            for (auto& p : players)
            {
                if (lc->env->IsSameObject(p.GetInstance(), localInst)) continue;

                Target t;
                Vec3 pos = p.getPosition();
                if (pos.GetInstance() == nullptr) {
                    // No position read — can't decide in-range either way.
                    // Force-clear glow so a previous "true" doesn't stick on
                    // an entity we can no longer evaluate this iteration.
                    if (glowNow) p.setGlowingTag(false);
                    continue;
                }
                t.x = pos.getX();
                t.y = pos.getY();
                t.z = pos.getZ();
                t.prevX = p.getXo();
                t.prevY = p.getYo();
                t.prevZ = p.getZo();

                double dx = t.x - back.cam.x, dy = t.y - back.cam.y, dz = t.z - back.cam.z;
                double distSq = dx*dx + dy*dy + dz*dz;
                if (distSq > maxDistSq) {
                    // Out of ESP range — clear so the glow shrinks with the
                    // ESP range setting rather than leaking past it.
                    if (glowNow) p.setGlowingTag(false);
                    continue;
                }

                AABB box = p.getBoundingBox();
                if (box.GetInstance() != nullptr)
                {
                    // Convert absolute AABB to extents centered on feet. Width
                    // and depth are symmetrical around the entity's X/Z; height
                    // is the full Y span from feet upward.
                    t.halfWidth = (box.maxX() - box.minX()) * 0.5;
                    t.height    =  box.maxY() - box.minY();
                    t.halfDepth = (box.maxZ() - box.minZ()) * 0.5;
                }
                else
                {
                    t.halfWidth = 0.3;
                    t.height    = 1.8;
                    t.halfDepth = 0.3;
                }

                // Pull team-formatted name with per-chunk colors so the
                // overlay can paint each Style-span (prefix / name / suffix)
                // in its own color the same way MC's renderer does.
                t.nameChunks = p.getFormattedNameChunks();

                // Bare GameProfile username for friend matching. Entity.getName()
                // on a Player returns a Component wrapping gameProfile.getName(),
                // so .getString() gives the raw "Notch" with no team prefix.
                {
                    Component bare = p.getName();
                    if (bare.GetInstance() != nullptr) {
                        t.playerName = bare.getString();
                        for (char& c : t.playerName)
                            c = (char)std::tolower((unsigned char)c);
                    }
                }

                // HP — for the nametag readout. Negative on failure; overlay
                // suppresses the chunk so a lookup hiccup doesn't print "-1/-1".
                t.health    = p.getHealth();
                t.maxHealth = p.getMaxHealth();

                // Friend lookup under the friends-list mutex. Hold the lock
                // for the linear scan only; the per-target bool then rides
                // with the snapshot so the overlay can render lock-free.
                if (!t.playerName.empty()) {
                    std::lock_guard<std::mutex> lk(g_settings.friendsMutex);
                    for (const auto& f : g_settings.friends) {
                        if (f == t.playerName) { t.isFriend = true; break; }
                    }
                }

                // formatNameForTeam emits chunks in order: [prefix] name [suffix].
                // The team's own color is applied to every chunk that doesn't
                // explicitly override it — guild prefixes override, the bare
                // player name does not. The last non-empty chunk is therefore
                // either the suffix or the name; either way it carries the
                // team color, which is what we want for the box.
                t.boxColor = 0xFFFFFFFFu;
                for (auto it = t.nameChunks.rbegin(); it != t.nameChunks.rend(); ++it) {
                    if (!it->first.empty()) { t.boxColor = it->second; break; }
                }

                // Glow application — true for in-range non-friend. We always
                // write so isFriend going true (user friend-toggles a target
                // mid-game) actively clears the outline rather than leaving
                // it sticky until they leave / re-enter range.
                if (glowNow) p.setGlowingTag(!t.isFriend);

                back.targets.push_back(std::move(t));
            }

            // One-shot cleanup pass on the falling edge of drawGlow. Writes
            // false to every non-local player so any in-flight "true" we left
            // last iteration gets actively cleared, instead of waiting for
            // each entity to despawn / leave render distance before MC stops
            // outlining them.
            if (s_glowWasOn && !glowNow) {
                for (auto& p : players) {
                    if (lc->env->IsSameObject(p.GetInstance(), localInst)) continue;
                    p.setGlowingTag(false);
                }
            }
            s_glowWasOn = glowNow;

            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();

            back.valid = true;

            lc->env->PopLocalFrame(nullptr);

            {
                std::lock_guard<std::mutex> lk(snapMutex);
                std::swap(snapshot, back);
            }
        }

        lc->vm->DetachCurrentThread();
        return 0;
    }
}
