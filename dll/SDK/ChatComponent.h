#pragma once
#include "Lunar.h"

class ChatComponent
{
public:
    ChatComponent(jobject instance);

    jclass GetClass();

    void Cleanup();

    jobject GetInstance();

private:
    jobject instance;
};
