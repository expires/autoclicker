#include "Lunar.h"

thread_local JNIEnv *Lunar::env = nullptr;

void Lunar::GetLoadedClasses()
{
    jvmtiEnv *jvmti;
    if (vm->GetEnv((void **)&jvmti, JVMTI_VERSION_1_2) != JNI_OK)
    {
        printf("[AC] Failed to get JVMTI env\n");
        return;
    }

    jclass lang = env->FindClass("java/lang/Class");
    jmethodID getName = env->GetMethodID(lang, "getName", "()Ljava/lang/String;");

    jclass *classesPtr;
    jint amount;

    jvmti->GetLoadedClasses(&amount, &classesPtr);
    printf("[AC] Loaded %d classes\n", amount);

    for (int i = 0; i < amount; i++)
    {
        jstring name = (jstring)env->CallObjectMethod(classesPtr[i], getName);
        const char *className = env->GetStringUTFChars(name, 0);
        // Promote to global so the ref stays valid across threads + JNI frames.
        // JVMTI returns local refs that would otherwise die when our outermost
        // frame ends; we also need to share these with the ESP thread.
        jclass global = (jclass)env->NewGlobalRef(classesPtr[i]);
        classes.emplace(std::make_pair((std::string)className, global));
        env->ReleaseStringUTFChars(name, className);
        env->DeleteLocalRef(classesPtr[i]);
    }
    classesLoaded.store(true, std::memory_order_release);
}

jclass Lunar::GetClass(std::string classname)
{
    if (classes.contains(classname))
        return classes.at(classname);

    printf("[AC] Class not found: %s\n", classname.c_str());
    return NULL;
}
