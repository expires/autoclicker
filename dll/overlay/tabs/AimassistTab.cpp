#include "Tabs.h"
#include "../OverlayWidgets.h"
#include "../../config/Settings.h"

namespace OverlayTabs
{
    bool RenderAimassist()
    {
        using namespace OverlayWidgets;
        bool dirty = false;
        dirty |= ModuleHeader("Enabled", &g_settings.aimEnabled, &g_settings.aimKey);
        if (g_settings.aimEnabled) {
            dirty |= RowCheckbox("Click Assist", &g_settings.aimClickOnly);
            {
                int mode = g_settings.aimMode;
                if (RowRadio("Mode", &mode, "Center\0Backstab\0")) {
                    g_settings.aimMode = mode;
                    dirty = true;
                }
            }
            dirty |= RowSlider  ("Horizontal Speed", &g_settings.aimSpeedH, 0, 20);
            dirty |= RowSlider  ("Vertical Speed",   &g_settings.aimSpeedV, 0, 20);
            dirty |= RowSlider  ("FOV (deg)",        &g_settings.aimFov,    1, 360);
            dirty |= RowSlider  ("Range (blocks)",   &g_settings.aimRange,  1, 64);
        }
        
        return dirty;
    }
}
