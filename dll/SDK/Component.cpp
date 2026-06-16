#include "Component.h"
#include "Mappings.h"

Component::Component(jobject instance)
{
    this->instance = instance;
}

jclass Component::GetClass()
{
    return lc->GetClass(MC_Component);
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
    jclass cls = lc->env->GetObjectClass(this->GetInstance());
    jmethodID getStringMethod = lc->env->GetMethodID(cls, MTD_Component_getString, "()Ljava/lang/String;");
    if (!getStringMethod || lc->env->ExceptionCheck())
    {
        lc->env->ExceptionClear();
        return "";
    }

    jstring javaString = (jstring)lc->env->CallObjectMethod(this->GetInstance(), getStringMethod);
    if (!javaString || lc->env->ExceptionCheck())
    {
        lc->env->ExceptionClear();
        return "";
    }

    const char *strChars = lc->env->GetStringUTFChars(javaString, nullptr);
    std::string result(strChars ? strChars : "");
    lc->env->ReleaseStringUTFChars(javaString, strChars);

    return result;
}
