#pragma once
#include "Lunar.h"
#include "Component.h"
#include <string>
#include <vector>

class ChatComponent
{
public:
    ChatComponent(jobject instance);

    jclass GetClass();

    void Cleanup();

    jobject GetInstance();

    std::vector<std::string> getRecentMessages(int max);

private:
    jobject instance;
};
