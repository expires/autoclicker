#include "JvmtiAgent.h"
#include "Lunar.h"
#include "Mappings.h"
#include "../Settings.h"
#include <cstring>
#include <string>

namespace JvmtiAgent
{
    static jvmtiEnv*  s_jvmti         = nullptr;
    static jmethodID  s_hookedMethod  = nullptr;
    static bool       s_breakpointSet = false;
    static int        s_step          = 0; // highest step Init() reached, 0..7
    static jvmtiError s_lastError     = JVMTI_ERROR_NONE;

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
        s_step      = 0;
        s_lastError = JVMTI_ERROR_NONE;

        if (lc->vm == nullptr) return false;

        if (lc->vm->GetEnv(reinterpret_cast<void**>(&s_jvmti), JVMTI_VERSION_1_2) != JNI_OK)
            return false;
        s_step = 1;

        jvmtiCapabilities caps;
        std::memset(&caps, 0, sizeof(caps));
        caps.can_generate_breakpoint_events = 1;
        caps.can_force_early_return         = 1;
        s_lastError = s_jvmti->AddCapabilities(&caps);
        if (s_lastError != JVMTI_ERROR_NONE) return false;
        s_step = 2;

        jvmtiEventCallbacks callbacks;
        std::memset(&callbacks, 0, sizeof(callbacks));
        callbacks.Breakpoint = &BreakpointCallback;
        s_lastError = s_jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
        if (s_lastError != JVMTI_ERROR_NONE) return false;
        s_step = 3;

        s_lastError = s_jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                                        JVMTI_EVENT_BREAKPOINT,
                                                        nullptr);
        if (s_lastError != JVMTI_ERROR_NONE) return false;
        s_step = 4;

        // Try our class map first; fall back to JNI FindClass (slash form) in
        // case AvatarRenderer was loaded after the autoclicker's initial sweep.
        jclass cls = lc->GetClass(MC_AvatarRenderer);
        if (!cls)
        {
            std::string slashForm = MC_AvatarRenderer;
            for (auto& ch : slashForm) if (ch == '.') ch = '/';
            cls = lc->env->FindClass(slashForm.c_str());
            if (!cls) { lc->env->ExceptionClear(); return false; }
        }
        s_step = 5;

        s_hookedMethod = lc->env->GetMethodID(cls,
            MTD_AvatarRenderer_shouldShowName,
            DESC_AvatarRenderer_shouldShowName);
        if (!s_hookedMethod)
        {
            lc->env->ExceptionClear();
            return false;
        }
        s_step = 6;

        s_lastError = s_jvmti->SetBreakpoint(s_hookedMethod, 0);
        if (s_lastError != JVMTI_ERROR_NONE) return false;
        s_step = 7;

        s_breakpointSet = true;
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
    int  GetStep()  { return s_step; }
    int  GetError() { return (int)s_lastError; }
}
