#pragma once
#include "Lunar.h"
#include "ChatComponent.h"

class Gui
{
public:
    Gui(jobject instance);

    jclass GetClass();

    void Cleanup();

    jobject GetInstance();

    ChatComponent getChat();

private:
    jobject instance;
};
