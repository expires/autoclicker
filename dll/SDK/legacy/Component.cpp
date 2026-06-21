#include "Component.h"
#include "Mappings.h"

std::string Component::getString()
{
    if (this->GetInstance() == nullptr) return "";

    {
        static jclass strCls = nullptr;
        if (!strCls)
        {
            jclass local = lc->env->FindClass("java/lang/String");
            if (local) { strCls = (jclass)lc->env->NewGlobalRef(local); lc->env->DeleteLocalRef(local); }
            else lc->env->ExceptionClear();
        }
        if (strCls && lc->env->IsInstanceOf(this->GetInstance(), strCls) == JNI_TRUE)
        {
            const char *c = lc->env->GetStringUTFChars((jstring)this->GetInstance(), nullptr);
            std::string r(c ? c : "");
            lc->env->ReleaseStringUTFChars((jstring)this->GetInstance(), c);
            return r;
        }
    }

    static jmethodID cachedMethod = nullptr;
    JMethod(cachedMethod, this->GetClass(), MTD_Component_getString, "()Ljava/lang/String;");
    jmethodID getStringMethod = cachedMethod;
    if (!getStringMethod)
    {
        jclass cls = lc->env->GetObjectClass(this->GetInstance());
        getStringMethod = lc->env->GetMethodID(cls, MTD_Component_getString, "()Ljava/lang/String;");
    }
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
