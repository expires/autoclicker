#include "Gui.h"
#include "Mappings.h"

Gui::Gui(jobject instance)
{
    this->instance = instance;
}

jclass Gui::GetClass()
{
    static jclass c = nullptr;
    return JClass(c, MC_Gui);
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
    static jmethodID getChat = nullptr;
    JMethod(getChat, this->GetClass(), MTD_Gui_getChat, DESC_Gui_getChat);
    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), getChat);

    if (rtn == nullptr)
        return nullptr;

    return ChatComponent(rtn);
}
