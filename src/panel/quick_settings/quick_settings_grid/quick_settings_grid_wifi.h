#pragma once

#include <NetworkManager.h>
#include <adwaita.h>

#include "quick_settings_grid_button.h"
#include "quick_settings_grid_wifi_menu.h"

#define QUICK_SETTINGS_WIFI_TYPE "wifi"

typedef struct _QSGridWifiButton {
    // embedd this as first argument so we can cast to it.
    QuickSettingsGridButton button;
    QuickSettingsGridWifiMenu *menu;
    NMDeviceWifi *dev;

} QuickSettingsGridWifiButton;

QuickSettingsGridWifiButton *quick_settings_grid_wifi_button_init(
    NMDeviceWifi *dev);

void quick_settings_grid_wifi_button_free(QuickSettingsGridWifiButton *self);

GtkWidget *quick_settings_grid_wifi_button_get_menu_widget(
    QuickSettingsGridWifiButton *self);

void quick_settings_grid_wifi_menu_on_reveal(
    QuickSettingsGridButton *button, gboolean is_revealed);