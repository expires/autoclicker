#pragma once
#include <cstdint>
#include "imgui.h"

namespace OverlayWidgets
{
    ImVec4 FromHex(uint32_t hex, float alpha = 1.0f);

    bool RowCheckbox(const char* label, bool* v);
    bool RowSlider(const char* label, int* v, int v_min, int v_max, const char* fmt = "%d");
    bool RowKeybind(const char* label, int* vk, bool allowMouse = false);
    bool RowInputInt(const char* label, int* v, int v_min, int v_max, int step = 1, int fastStep = 10);
    bool RowInputIntPair(const char* labelA, int* vA, int minA, int maxA, int stepA, int fastA,
                         const char* labelB, int* vB, int minB, int maxB, int stepB, int fastB);
    bool SidebarTab(const char* label, bool selected);

    bool IsKeybindListening();
    void ResetKeybindListening();
}
