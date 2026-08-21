#pragma once
#include "Lunar.h"
#include "Component.h"
#include <string>
#include <vector>

struct ChatMessage
{
    int         addedTime = 0;
    std::string text;
};

class ChatComponent
{
public:
    ChatComponent(jobject instance);

    jclass GetClass();

    void Cleanup();

    jobject GetInstance();

    std::vector<ChatMessage> getRecentMessages(int max);

private:
    jobject instance;
};
