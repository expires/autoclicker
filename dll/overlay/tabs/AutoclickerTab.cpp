#include "Tabs.h"
#include "../OverlayWidgets.h"
#include "../../config/Settings.h"

namespace OverlayTabs
{
    bool RenderAutoclicker()
    {
        using namespace OverlayWidgets;
        bool dirty = false;
        dirty |= ModuleHeader("Enabled", &g_settings.acEnabled, &g_settings.acKey);
        if (g_settings.acEnabled) {
            int mode = g_settings.clickerMode;
            if (RowRadio("Mode", &mode, "Normal\0Extra\0Extra+\0")) {
                g_settings.clickerMode = mode;
                dirty = true;
            }
            dirty |= RowCheckbox("Break Blocks",    &g_settings.breakBlocks);
            dirty |= RowCheckbox("Inventory Click", &g_settings.inventoryClick);
            dirty |= RowRangeSlider("CPS", &g_settings.cpsMin, &g_settings.cpsMax, 1, 20);
            dirty |= RowCheckbox("Jitter",       &g_settings.jitterEnabled);
            if (g_settings.jitterEnabled)
                dirty |= RowSlider("Strength", &g_settings.jitterStrength, 0, 10);
        }
        
        return dirty;
    }
}
