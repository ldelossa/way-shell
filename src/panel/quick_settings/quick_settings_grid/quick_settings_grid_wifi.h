#include <NetworkManager.h>
#include <adwaita.h>

#include "quick_settings_grid_button.h"

#define QUICK_SETTINGS_WIFI_TYPE "wifi"

typedef struct _QSGridWifiMenu QuickSettingsGridWifiMenu;

typedef struct _QSGridWifiButton {
    // embedd this as first argument so we can cast to it.
    QuickSettingsGridButton button;
    QuickSettingsGridWifiMenu *menu;
    NMDeviceWifi *dev;
} QuickSettingsGridWifiButton;

typedef struct _QSGridWifiMenu {
    QuickSettingsGridWifiButton *button;
    GtkBox container;
    GtkRevealer *revealer;
} QuickSettingsGridWifiMenu;

QuickSettingsGridWifiButton *quick_settings_grid_wifi_button_init(
    NMDeviceWifi *dev);

void quick_settings_grid_wifi_button_free(QuickSettingsGridWifiButton *self);
