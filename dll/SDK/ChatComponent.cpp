#include "ChatComponent.h"

ChatComponent::ChatComponent(jobject instance)
{
    this->instance = instance;
}

jclass ChatComponent::GetClass()
{
    return lc->GetClass("net.minecraft.class_338");
}

void ChatComponent::Cleanup()
{
    lc->env->DeleteLocalRef(this->instance);
}

jobject ChatComponent::GetInstance()
{
    return this->instance;
}
