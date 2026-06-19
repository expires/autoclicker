#pragma once
#include <string>

namespace Notifications
{
    enum class Kind { Info, Enabled, Disabled, Alert };

    void Push(const std::string& text, Kind kind = Kind::Info);
    void Render(float dispW, float dispH);
    bool HasActive();
}
