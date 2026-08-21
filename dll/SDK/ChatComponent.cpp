#include "ChatComponent.h"
#include "Mappings.h"

ChatComponent::ChatComponent(jobject instance)
{
    this->instance = instance;
}

jclass ChatComponent::GetClass()
{
    static jclass c = nullptr;
    return JClass(c, MC_ChatComponent);
}

void ChatComponent::Cleanup()
{
    lc->env->DeleteLocalRef(this->instance);
}

jobject ChatComponent::GetInstance()
{
    return this->instance;
}

std::vector<std::string> ChatComponent::getRecentMessages(int max)
{
    std::vector<std::string> out;
    if (this->instance == nullptr || max <= 0)          return out;
    if (MTD_ChatMessage_content[0] == '\0')             return out;

    static jfieldID messagesField = nullptr;
    JField(messagesField, this->GetClass(), FLD_ChatComponent_messages, DESC_ChatComponent_messages);
    if (!messagesField) { lc->env->ExceptionClear(); return out; }

    jobject list = lc->env->GetObjectField(this->instance, messagesField);
    if (!list || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return out; }

    static jclass    listCls = nullptr;
    static jmethodID sizeM   = nullptr;
    static jmethodID getM    = nullptr;
    if (!JListOps(listCls, sizeM, getM)) { lc->env->DeleteLocalRef(list); return out; }

    static jclass    messageCls = nullptr;
    static jmethodID contentM   = nullptr;
    JMethod(contentM, JClass(messageCls, MC_ChatMessage),
            MTD_ChatMessage_content, DESC_ChatMessage_content);
    if (!contentM) { lc->env->ExceptionClear(); lc->env->DeleteLocalRef(list); return out; }

    const jint size  = lc->env->CallIntMethod(list, sizeM);
    const jint count = size < (jint)max ? size : (jint)max;
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); lc->env->DeleteLocalRef(list); return out; }

    out.reserve(count);
    for (jint i = 0; i < count; ++i)
    {
        jobject message = lc->env->CallObjectMethod(list, getM, i);
        if (!message || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); continue; }

        jobject content = lc->env->CallObjectMethod(message, contentM);
        if (content && !lc->env->ExceptionCheck())
        {
            Component text(content);
            out.push_back(text.getString());
            lc->env->DeleteLocalRef(content);
        }
        else
        {
            lc->env->ExceptionClear();
        }

        lc->env->DeleteLocalRef(message);
    }

    lc->env->DeleteLocalRef(list);
    return out;
}
