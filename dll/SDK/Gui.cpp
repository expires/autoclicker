#include "Gui.h"

Gui::Gui(jobject instance)
{
    this->instance = instance;
}

jclass Gui::GetClass()
{
    return lc->GetClass("net.minecraft.client.gui.Gui");
}

void Gui::Cleanup()
{
    lc->env->DeleteLocalRef(this->instance);
}

jobject Gui::GetInstance()
{
    return this->instance;
}

ChatComponent Gui::getChat()
{
    jmethodID getChat = lc->env->GetMethodID(this->GetClass(), "getChat", "()Lnet/minecraft/client/gui/components/ChatComponent;");
    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), getChat);

    if (rtn == nullptr)
        return nullptr;

    return ChatComponent(rtn);
}