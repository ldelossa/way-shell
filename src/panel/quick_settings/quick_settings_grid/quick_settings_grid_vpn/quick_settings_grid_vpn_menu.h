#pragma once

#include <NetworkManager.h>
#include <adwaita.h>

G_BEGIN_DECLS

struct _QuickSettingsGridVPNMenu;
#define QUICK_SETTINGS_GRID_VPN_MENU_TYPE \
    quick_settings_grid_vpn_menu_get_type()
G_DECLARE_FINAL_TYPE(QuickSettingsGridVPNMenu, quick_settings_grid_vpn_menu,
                     QUICK_SETTINGS, GRID_VPN_MENU, GObject);

G_END_DECLS

GtkWidget *quick_settings_grid_vpn_menu_get_widget(
    QuickSettingsGridVPNMenu *grid);
