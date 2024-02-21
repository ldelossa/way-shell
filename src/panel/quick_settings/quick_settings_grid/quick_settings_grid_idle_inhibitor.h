
#include <adwaita.h>

#include "quick_settings_grid_button.h"

#define QUICK_SETTINGS_IDLE_INHIBITOR_TYPE "idle_inhibitor"

typedef struct _QuickSettingsGridIdleInhibitorMenu
    QuickSettingsGridIdleInhibitorMenu;

typedef struct _QuickSettingsGridIdleInhibitorButton {
    // embedd this as first argument so we can cast to it.
    QuickSettingsGridButton button;
} QuickSettingsGridIdleInhibitorButton;

QuickSettingsGridIdleInhibitorButton *
quick_settings_grid_idle_inhibitor_button_init();

void quick_settings_grid_inhibitor_button_free(
    QuickSettingsGridIdleInhibitorButton *self);
