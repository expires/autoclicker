#include "Lunar.h"

void Lunar::GetLoadedClasses()
{
    jvmtiEnv *jvmti;
    if (vm->GetEnv((void **)&jvmti, JVMTI_VERSION_1_2) != JNI_OK)
        return;

    jclass lang = env->FindClass("java/lang/Class");
    jmethodID getName = env->GetMethodID(lang, "getName", "()Ljava/lang/String;");

    jclass *classesPtr;
    jint amount;

    jvmti->GetLoadedClasses(&amount, &classesPtr);
    for (int i = 0; i < amount; i++)
    {
        jstring name = (jstring)env->CallObjectMethod(classesPtr[i], getName);
        const char *className = env->GetStringUTFChars(name, 0);
        env->ReleaseStringUTFChars(name, className);
        classes.emplace(std::make_pair((std::string)className, classesPtr[i]));
    }
}

jclass Lunar::GetClass(std::string classname)
{
    if (classes.contains(classname))
        return classes.at(classname);

    return NULL;
}