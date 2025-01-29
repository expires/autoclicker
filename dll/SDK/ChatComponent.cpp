#include "ChatComponent.h"

ChatComponent::ChatComponent(jobject instance)
{
    this->ccInstance = instance;
}

jclass ChatComponent::GetClass()
{
    return lc->GetClass("net.minecraft.client.gui.components.ChatComponent");
}

void ChatComponent::Cleanup()
{
    lc->env->DeleteLocalRef(this->ccInstance);
}

jobject ChatComponent::GetInstance()
{
    return this->ccInstance;
}
