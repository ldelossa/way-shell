#include <NetworkManager.h>
#include <adwaita.h>

#include "quick_settings_grid_button.h"

#define QUICK_SETTINGS_ETHERNET_TYPE "ethernet"

typedef struct _QuickSettingsGridEthernetMenu QuickSettingsGridEthernetMenu;

typedef struct _QuickSettingsGridEthernetButton {
    // embedd this as first argument so we can cast to it.
    QuickSettingsGridButton button;
    QuickSettingsGridEthernetMenu *menu;
    NMDeviceEthernet *dev;
} QuickSettingsGridEthernetButton;

typedef struct _QuickSettingsGridEthernetMenu {
    QuickSettingsGridEthernetButton *button;
    GtkBox container;
    GtkRevealer *revealer;
} QuickSettingsGridEthernetMenu;

QuickSettingsGridEthernetButton *quick_settings_grid_ethernet_button_init(
    NMDeviceEthernet *dev);

void quick_settings_grid_ethernet_button_free(
    QuickSettingsGridEthernetButton *self);
