#include "ChatComponent.h"
#include "Mappings.h"

ChatComponent::ChatComponent(jobject instance)
{
    this->instance = instance;
}

jclass ChatComponent::GetClass()
{
    return lc->GetClass(MC_ChatComponent);
}

void ChatComponent::Cleanup()
{
    lc->env->DeleteLocalRef(this->instance);
}

jobject ChatComponent::GetInstance()
{
    return this->instance;
}
