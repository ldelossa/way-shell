#pragma once

#include <NetworkManager.h>
#include <adwaita.h>

#include "../quick_settings_grid_button.h"
#include "quick_settings_grid_vpn_menu.h"

#define QUICK_SETTINGS_VPN_TYPE "vpn"

typedef struct _QuickSesstingsGridVPNButton {
    // embedd this as first argument so we can cast to it.
    QuickSettingsGridButton button;
    QuickSettingsGridVPNMenu *menu;
    gboolean enabled;
	GPtrArray *active_stack;

} QuickSettingsGridVPNButton;

QuickSettingsGridVPNButton *quick_settings_grid_vpn_button_init();

void quick_settings_grid_vpn_button_free(QuickSettingsGridVPNButton *self);

GtkWidget *quick_settings_grid_vpn_button_get_menu_widget(
    QuickSettingsGridVPNButton *self);

void quick_settings_grid_vpn_menu_on_reveal(
    QuickSettingsGridButton *button, gboolean is_revealed);
