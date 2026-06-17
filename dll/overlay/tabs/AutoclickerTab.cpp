#include "Tabs.h"
#include "../OverlayWidgets.h"
#include "../../config/Settings.h"

namespace OverlayTabs
{
    bool RenderAutoclicker()
    {
        using namespace OverlayWidgets;
        bool dirty = false;
        dirty |= RowCheckbox("Enabled",      &g_settings.acEnabled);
        dirty |= RowCheckbox("Break Blocks", &g_settings.breakBlocks);
        dirty |= RowSlider  ("CPS",          &g_settings.cps, 1, 20);
        dirty |= RowCheckbox("Jitter",       &g_settings.jitterEnabled);
        if (g_settings.jitterEnabled)
            dirty |= RowSlider("Strength", &g_settings.jitterStrength, 0, 10);
        return dirty;
    }
}
