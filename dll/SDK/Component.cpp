#include "Component.h"

Component::Component(jobject instance)
{
    this->instance = instance;
}

jclass Component::GetClass()
{
    return lc->GetClass("net.minecraft.network.chat.Component");
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
    jmethodID getStringMethod = lc->env->GetMethodID(this->GetClass(), "getString", "()Ljava/lang/String;");
    jstring javaString = (jstring)lc->env->CallObjectMethod(this->GetInstance(), getStringMethod);

    const char *strChars = lc->env->GetStringUTFChars(javaString, nullptr);
    std::string result(strChars);
    lc->env->ReleaseStringUTFChars(javaString, strChars);

    return result;
}
