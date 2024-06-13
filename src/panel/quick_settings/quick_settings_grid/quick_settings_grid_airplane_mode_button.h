#include <adwaita.h>

#include "quick_settings_grid_button.h"

#define QUICK_SETTINGS_AIRPLANE_MODE_TYPE "airplane_mode"

typedef struct _QuickSettingsGridAirplaneModeMenu
    QuickSettingsGridAirplaneModeMenu;

typedef struct _QuickSettingsGridAirplaneModeButton {
    // embedd this as first argument so we can cast to it.
    QuickSettingsGridButton button;
} QuickSettingsGridAirplaneModeButton;

QuickSettingsGridAirplaneModeButton *
quick_settings_grid_airplane_mode_button_init();

void quick_settings_grid_airplane_mode_button_free(
    QuickSettingsGridAirplaneModeButton *self);
