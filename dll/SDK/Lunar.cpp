#include "Lunar.h"

void Lunar::GetLoadedClasses()
{
    jvmtiEnv *jvmti;
    jint jvmtiResult = vm->GetEnv((void **)&jvmti, JVMTI_VERSION_1_2);
    if (jvmtiResult != JNI_OK)
    {
        printf("[AC] Failed to get JVMTI env (result=%d)\n", jvmtiResult);
        return;
    }
    printf("[AC] Got JVMTI env\n");

    jclass lang = env->FindClass("java/lang/Class");
    jmethodID getName = env->GetMethodID(lang, "getName", "()Ljava/lang/String;");

    jclass *classesPtr;
    jint amount;

    jvmti->GetLoadedClasses(&amount, &classesPtr);
    printf("[AC] Total loaded classes: %d\n", amount);

    for (int i = 0; i < amount; i++)
    {
        jstring name = (jstring)env->CallObjectMethod(classesPtr[i], getName);
        const char *className = env->GetStringUTFChars(name, 0);
        classes.emplace(std::make_pair((std::string)className, classesPtr[i]));
        env->ReleaseStringUTFChars(name, className);
    }

    printf("[AC] Classes stored in map: %zu\n", classes.size());
    // Print first 10 minecraft-related class names to verify naming
    int printed = 0;
    for (const auto& [name, cls] : classes)
    {
        if (name.find("minecraft") != std::string::npos && printed < 10)
        {
            printf("[AC] Sample class: %s\n", name.c_str());
            printed++;
        }
    }
}

jclass Lunar::GetClass(std::string classname)
{
    if (classes.contains(classname))
        return classes.at(classname);

    return NULL;
}