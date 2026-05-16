#include "JvmtiAgent.h"
#include "Lunar.h"
#include "Mappings.h"
#include "../Settings.h"
#include <cstdio>
#include <cstring>

namespace JvmtiAgent
{
    static jvmtiEnv*  s_jvmti        = nullptr;
    static jmethodID  s_hookedMethod = nullptr;
    static bool       s_breakpointSet = false;

    // Called by the JVM on whichever thread hits the breakpoint (typically the
    // render thread). Must be fast — it runs synchronously, blocking that thread
    // until we return.
    static void JNICALL BreakpointCallback(jvmtiEnv* jvmti_env,
                                           JNIEnv* /*jni_env*/,
                                           jthread thread,
                                           jmethodID method,
                                           jlocation /*location*/)
    {
        if (method != s_hookedMethod) return;
        if (!g_settings.espEnabled || !g_settings.drawName) return;

        // shouldShowName returns boolean. JVM uses int for boolean on the stack.
        jvmti_env->ForceEarlyReturnInt(thread, 0);
    }

    bool Init()
    {
        if (lc->vm == nullptr) { printf("[AC] JvmtiAgent: no JVM\n"); return false; }

        if (lc->vm->GetEnv(reinterpret_cast<void**>(&s_jvmti), JVMTI_VERSION_1_2) != JNI_OK)
        {
            printf("[AC] JvmtiAgent: GetEnv(JVMTI) failed\n");
            return false;
        }

        // Request the capabilities we need. Both are "live phase OK" per JVMTI
        // spec, so we can add them after the JVM has started.
        jvmtiCapabilities caps;
        std::memset(&caps, 0, sizeof(caps));
        caps.can_generate_breakpoint_events = 1;
        caps.can_force_early_return         = 1;
        jvmtiError err = s_jvmti->AddCapabilities(&caps);
        if (err != JVMTI_ERROR_NONE)
        {
            printf("[AC] JvmtiAgent: AddCapabilities failed: %d\n", err);
            return false;
        }

        jvmtiEventCallbacks callbacks;
        std::memset(&callbacks, 0, sizeof(callbacks));
        callbacks.Breakpoint = &BreakpointCallback;
        err = s_jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
        if (err != JVMTI_ERROR_NONE)
        {
            printf("[AC] JvmtiAgent: SetEventCallbacks failed: %d\n", err);
            return false;
        }

        err = s_jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                                JVMTI_EVENT_BREAKPOINT,
                                                nullptr);
        if (err != JVMTI_ERROR_NONE)
        {
            printf("[AC] JvmtiAgent: SetEventNotificationMode failed: %d\n", err);
            return false;
        }

        // Find the method ID using whatever JNIEnv this calling thread has.
        jclass cls = lc->GetClass(MC_AvatarRenderer);
        if (!cls)
        {
            printf("[AC] JvmtiAgent: AvatarRenderer class not in map\n");
            return false;
        }

        s_hookedMethod = lc->env->GetMethodID(cls,
            MTD_AvatarRenderer_shouldShowName,
            DESC_AvatarRenderer_shouldShowName);
        if (!s_hookedMethod)
        {
            lc->env->ExceptionClear();
            printf("[AC] JvmtiAgent: shouldShowName method not found\n");
            return false;
        }

        err = s_jvmti->SetBreakpoint(s_hookedMethod, 0);
        if (err != JVMTI_ERROR_NONE)
        {
            printf("[AC] JvmtiAgent: SetBreakpoint failed: %d\n", err);
            return false;
        }

        s_breakpointSet = true;
        printf("[AC] JvmtiAgent: hooked AvatarRenderer.shouldShowName\n");
        return true;
    }

    void Shutdown()
    {
        if (s_jvmti == nullptr) return;
        if (s_breakpointSet && s_hookedMethod)
        {
            s_jvmti->ClearBreakpoint(s_hookedMethod, 0);
            s_breakpointSet = false;
        }
        s_jvmti->SetEventNotificationMode(JVMTI_DISABLE,
                                          JVMTI_EVENT_BREAKPOINT,
                                          nullptr);
    }

    bool IsActive() { return s_breakpointSet; }
}
