#pragma once
#include <jni.h>
#include <jvmti.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <string>

class Lunar
{
public:
	static thread_local JNIEnv *env;
	JavaVM *vm{nullptr};
	std::atomic<bool> classesLoaded{false};

public:
	void GetLoadedClasses();
	jclass GetClass(const std::string &classname);

private:
	void RefreshLocked();

	std::unordered_map<std::string, jclass> classes;
	std::mutex classesMutex;
	std::chrono::steady_clock::time_point lastRefresh{};
};

inline auto lc = std::make_unique<Lunar>();

inline jclass JClass(jclass &slot, const char *name)
{
    if (!slot)
        slot = lc->GetClass(name);
    return slot;
}

inline jfieldID JField(jfieldID &slot, jclass cls, const char *name, const char *sig)
{
    if (!slot && cls)
    {
        slot = lc->env->GetFieldID(cls, name, sig);
        if (!slot)
            lc->env->ExceptionClear();
    }
    return slot;
}

inline jfieldID JStaticField(jfieldID &slot, jclass cls, const char *name, const char *sig)
{
    if (!slot && cls)
    {
        slot = lc->env->GetStaticFieldID(cls, name, sig);
        if (!slot)
            lc->env->ExceptionClear();
    }
    return slot;
}

inline jmethodID JMethod(jmethodID &slot, jclass cls, const char *name, const char *sig)
{
    if (!slot && cls)
    {
        slot = lc->env->GetMethodID(cls, name, sig);
        if (!slot)
            lc->env->ExceptionClear();
    }
    return slot;
}

inline jmethodID JStaticMethod(jmethodID &slot, jclass cls, const char *name, const char *sig)
{
    if (!slot && cls)
    {
        slot = lc->env->GetStaticMethodID(cls, name, sig);
        if (!slot)
            lc->env->ExceptionClear();
    }
    return slot;
}

inline bool JListOps(jclass &cls, jmethodID &sizeM, jmethodID &getM)
{
    if (!cls)
    {
        jclass local = lc->env->FindClass("java/util/List");
        if (!local)
        {
            lc->env->ExceptionClear();
            return false;
        }
        cls = (jclass)lc->env->NewGlobalRef(local);
        lc->env->DeleteLocalRef(local);
    }
    JMethod(sizeM, cls, "size", "()I");
    JMethod(getM, cls, "get", "(I)Ljava/lang/Object;");
    return cls && sizeM && getM;
}
