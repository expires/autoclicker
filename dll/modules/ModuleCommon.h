#pragma once
#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>
#include <thread>
#include "../SDK/Lunar.h"
#include "autoclicker/AutoclickerModule.h"

namespace ModuleCommon
{
    inline bool AttachToJvm()
    {
        while (!AutoclickerModule::destruct &&
               !(lc->vm != nullptr && lc->classesLoaded.load(std::memory_order_acquire)))
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (AutoclickerModule::destruct) return false;

        if (lc->vm->AttachCurrentThread(reinterpret_cast<void**>(&lc->env), nullptr) != JNI_OK)
            return false;
        return lc->env != nullptr;
    }

    inline bool ContainsCi(const std::string& hay, const char* needle)
    {
        if (!needle || !*needle) return false;
        std::string h = hay;
        std::string n = needle;
        std::transform(h.begin(), h.end(), h.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });
        std::transform(n.begin(), n.end(), n.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });
        return h.find(n) != std::string::npos;
    }
}
