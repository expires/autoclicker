#include "Component.h"

Component::Component(jobject instance)
{
    this->instance = instance;
}

jclass Component::GetClass()
{
    return lc->GetClass("net.minecraft.class_2561");
}

void Component::Cleanup()
{
    lc->env->DeleteLocalRef(this->instance);
}

jobject Component::GetInstance()
{
    return this->instance;
}

std::string Component::getString()
{
    jmethodID getStringMethod = lc->env->GetMethodID(this->GetClass(), "method_74062", "()Ljava/lang/String;");
    jstring javaString = (jstring)lc->env->CallObjectMethod(this->GetInstance(), getStringMethod);

    const char *strChars = lc->env->GetStringUTFChars(javaString, nullptr);
    std::string result(strChars);
    lc->env->ReleaseStringUTFChars(javaString, strChars);

    return result;
}
