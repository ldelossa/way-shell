#include <adwaita.h>

#include "quick_settings_grid_button.h"

// A simple QuickSettingsGridButton which removes itself from the cluster once
// clicked.
typedef struct _QuickSettingsGridOneShotButton {
    // embedd this as first argument so we can cast to it.
    QuickSettingsGridButton button;
} QuickSettingsGridOneShotButton;

QuickSettingsGridOneShotButton *quick_settings_grid_oneshot_button_init(
    const char *title, const char *subtitle, const char *icon);

void quick_settings_grid_oneshot_button_free(
    QuickSettingsGridOneShotButton *self);
