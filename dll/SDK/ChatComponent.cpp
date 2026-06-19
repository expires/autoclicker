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
