#include <adwaita.h>

#include "quick_settings_grid_button.h"

typedef struct _QuickSettingsGridOneThemeButton {
    // embedd this as first argument so we can cast to it.
    QuickSettingsGridButton button;
} QuickSettingsGridOneThemeButton;

QuickSettingsGridOneThemeButton *quick_settings_grid_theme_button_init();

void quick_settings_grid_theme_button_free(
    QuickSettingsGridOneThemeButton *self);
