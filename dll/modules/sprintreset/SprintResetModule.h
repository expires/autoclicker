#pragma once
#include <Windows.h>

namespace SprintResetModule {
    inline constexpr ULONG_PTR kInjectedExtraInfo = 0x4D4E4353;

    void NotePhysicalKey(WPARAM vk, bool down);
    void PreClick(bool entityHit);
    void PostClick();
}
