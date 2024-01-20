#pragma once

#include <NetworkManager.h>
#include <adwaita.h>

#include "../quick_settings_grid_button.h"
#include "quick_settings_grid_power_profiles_menu.h"

#define QUICK_SETTINGS_POWER_PROFILES_TYPE "power-profiles"

typedef struct _QuickSesstingsGridPowerProfilesButton {
    // embedd this as first argument so we can cast to it.
    QuickSettingsGridButton button;
    QuickSettingsGridPowerProfilesMenu *menu;
} QuickSettingsGridPowerProfilesButton;

QuickSettingsGridPowerProfilesButton *
quick_settings_grid_power_profiles_button_init();

void quick_settings_grid_power_profiles_button_free(
    QuickSettingsGridPowerProfilesButton *self);

GtkWidget *quick_settings_grid_power_profiles_button_get_menu_widget(
    QuickSettingsGridPowerProfilesButton *self);

void quick_settings_grid_power_profiles_menu_on_reveal(
    QuickSettingsGridButton *button, gboolean is_revealed);