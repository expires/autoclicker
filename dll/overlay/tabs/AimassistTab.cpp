#include "Tabs.h"
#include "../OverlayWidgets.h"
#include "../../Settings.h"

namespace OverlayTabs
{
    bool RenderAimassist()
    {
        using namespace OverlayWidgets;
        bool dirty = false;
        dirty |= RowCheckbox("Enabled",      &g_settings.aimEnabled);
        dirty |= RowCheckbox("Click Assist", &g_settings.aimClickOnly);
        dirty |= RowSlider  ("Horizontal Speed", &g_settings.aimSpeedH, 0, 20);
        dirty |= RowSlider  ("Vertical Speed",   &g_settings.aimSpeedV, 0, 20);
        dirty |= RowSlider  ("FOV (deg)",        &g_settings.aimFov,    1, 180);
        dirty |= RowSlider  ("Range (blocks)",   &g_settings.aimRange,  1, 64);
        return dirty;
    }
}
