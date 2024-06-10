#pragma once

#include <NetworkManager.h>
#include <adwaita.h>

#include "../quick_settings_grid_button.h"
#include "quick_settings_grid_night_light_menu.h"

#define QUICK_SETTINGS_NIGHT_LIGHT_TYPE "night-light"

typedef struct _QuickSesstingsGridNightLightButton {
    // embedd this as first argument so we can cast to it.
    QuickSettingsGridButton button;
    QuickSettingsGridNightLightMenu *menu;
    gboolean enabled;

} QuickSettingsGridNightLightButton;

QuickSettingsGridNightLightButton *quick_settings_grid_night_light_button_init();

void quick_settings_grid_night_light_button_free(QuickSettingsGridNightLightButton *self);

GtkWidget *quick_settings_grid_night_light_button_get_menu_widget(
    QuickSettingsGridNightLightButton *self);

void quick_settings_grid_night_light_menu_on_reveal(
    QuickSettingsGridButton *button, gboolean is_revealed);
