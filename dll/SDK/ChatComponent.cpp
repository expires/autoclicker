#include "ChatComponent.h"

ChatComponent::ChatComponent(jobject instance)
{
    this->instance = instance;
}

jclass ChatComponent::GetClass()
{
    return lc->GetClass("net.minecraft.client.gui.components.ChatComponent");
}

void ChatComponent::Cleanup()
{
    lc->env->DeleteLocalRef(this->instance);
}

jobject ChatComponent::GetInstance()
{
    return this->instance;
}
