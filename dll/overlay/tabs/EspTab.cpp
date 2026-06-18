#include "Tabs.h"
#include "../OverlayWidgets.h"
#include "../../config/Settings.h"

namespace OverlayTabs
{
    bool RenderEsp()
    {
        using namespace OverlayWidgets;
        bool dirty = false;
        dirty |= ModuleHeader("Enabled", &g_settings.espEnabled, &g_settings.espKey);
        if (g_settings.espEnabled) {
            dirty |= RowCheckbox("Box",                &g_settings.drawBox);
            dirty |= RowCheckbox("Name",               &g_settings.drawName);
            dirty |= RowCheckbox("Distance",           &g_settings.drawDistance);
            dirty |= RowCheckbox("Health",             &g_settings.drawHealth);
            dirty |= RowCheckbox("Highlight Friends",  &g_settings.highlightFriends);
        }
        
        return dirty;
    }
}
