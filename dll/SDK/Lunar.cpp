#include "Lunar.h"
#include "../logger/Logger.h"
#include <algorithm>
#include <fstream>
#include <vector>

thread_local JNIEnv *Lunar::env = nullptr;

void Lunar::RefreshLocked()
{
    if (env == nullptr || vm == nullptr)
        return;

    jvmtiEnv *jvmti = nullptr;
    if (vm->GetEnv((void **)&jvmti, JVMTI_VERSION_1_2) != JNI_OK || jvmti == nullptr)
        return;

    jclass lang = env->FindClass("java/lang/Class");
    if (lang == nullptr) { env->ExceptionClear(); jvmti->DisposeEnvironment(); return; }
    jmethodID getName = env->GetMethodID(lang, "getName", "()Ljava/lang/String;");
    if (getName == nullptr) { env->ExceptionClear(); jvmti->DisposeEnvironment(); return; }

    jclass *classesPtr = nullptr;
    jint amount = 0;
    if (jvmti->GetLoadedClasses(&amount, &classesPtr) != JVMTI_ERROR_NONE || classesPtr == nullptr)
    {
        jvmti->DisposeEnvironment();
        return;
    }

    for (jint i = 0; i < amount; i++)
    {
        jstring name = (jstring)env->CallObjectMethod(classesPtr[i], getName);
        if (name != nullptr)
        {
            const char *className = env->GetStringUTFChars(name, nullptr);
            if (className != nullptr)
            {
                std::string key = className;
                if (classes.find(key) == classes.end())
                    classes.emplace(std::move(key), (jclass)env->NewGlobalRef(classesPtr[i]));
                env->ReleaseStringUTFChars(name, className);
            }
            env->DeleteLocalRef(name);
        }
        env->DeleteLocalRef(classesPtr[i]);
    }

    jvmti->Deallocate(reinterpret_cast<unsigned char *>(classesPtr));
    jvmti->DisposeEnvironment();
    if (env->ExceptionCheck())
        env->ExceptionClear();
}

void Lunar::GetLoadedClasses()
{
    std::lock_guard<std::mutex> lk(classesMutex);
    RefreshLocked();
    lastRefresh = std::chrono::steady_clock::now();
    classesLoaded.store(true, std::memory_order_release);
    AC_LOG("lunar: GetLoadedClasses cached %zu classes", classes.size());
}

void Lunar::DumpLoadedClasses(const std::string &path)
{
    if (env == nullptr || vm == nullptr)
        return;

    jvmtiEnv *jvmti = nullptr;
    if (vm->GetEnv((void **)&jvmti, JVMTI_VERSION_1_2) != JNI_OK || jvmti == nullptr)
        return;

    jclass lang = env->FindClass("java/lang/Class");
    if (lang == nullptr) { env->ExceptionClear(); jvmti->DisposeEnvironment(); return; }
    jmethodID getName = env->GetMethodID(lang, "getName", "()Ljava/lang/String;");
    if (getName == nullptr) { env->ExceptionClear(); jvmti->DisposeEnvironment(); return; }

    jclass *classesPtr = nullptr;
    jint amount = 0;
    if (jvmti->GetLoadedClasses(&amount, &classesPtr) != JVMTI_ERROR_NONE || classesPtr == nullptr)
    {
        jvmti->DisposeEnvironment();
        return;
    }

    std::vector<std::string> names;
    names.reserve((size_t)amount);
    for (jint i = 0; i < amount; i++)
    {
        jstring name = (jstring)env->CallObjectMethod(classesPtr[i], getName);
        if (name != nullptr)
        {
            const char *chars = env->GetStringUTFChars(name, nullptr);
            if (chars != nullptr)
            {
                names.emplace_back(chars);
                env->ReleaseStringUTFChars(name, chars);
            }
            env->DeleteLocalRef(name);
        }
        env->DeleteLocalRef(classesPtr[i]);
    }

    jvmti->Deallocate(reinterpret_cast<unsigned char *>(classesPtr));
    jvmti->DisposeEnvironment();
    if (env->ExceptionCheck())
        env->ExceptionClear();

    std::sort(names.begin(), names.end());

    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open())
    {
        AC_LOG("lunar: DumpLoadedClasses could not open %s", path.c_str());
        return;
    }
    out << names.size() << " loaded classes\n";
    for (const auto &n : names)
        out << n << '\n';
    out.close();

    AC_LOG("lunar: dumped %zu class names to %s", names.size(), path.c_str());
}

jclass Lunar::GetClass(const std::string &classname)
{
    std::lock_guard<std::mutex> lk(classesMutex);

    auto it = classes.find(classname);
    if (it != classes.end())
        return it->second;

    const auto now = std::chrono::steady_clock::now();
    if (now - lastRefresh >= std::chrono::milliseconds(250))
    {
        AC_LOG("lunar: GetClass miss '%s', refreshing", classname.c_str());
        lastRefresh = now;
        RefreshLocked();
        it = classes.find(classname);
        if (it != classes.end())
            return it->second;
        AC_LOG("lunar: GetClass '%s' still missing after refresh", classname.c_str());
    }

    return nullptr;
}
