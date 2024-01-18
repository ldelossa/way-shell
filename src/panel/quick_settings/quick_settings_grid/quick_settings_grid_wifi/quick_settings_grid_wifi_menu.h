#pragma once

#include <NetworkManager.h>
#include <adwaita.h>

G_BEGIN_DECLS

struct _QuickSettingsGridWifiMenu;
#define QUICK_SETTINGS_GRID_WIFI_MENU_TYPE \
    quick_settings_grid_wifi_menu_get_type()
G_DECLARE_FINAL_TYPE(QuickSettingsGridWifiMenu, quick_settings_grid_wifi_menu,
                     QUICK_SETTINGS, GRID_WIFI_MENU, GObject);

G_END_DECLS

void quick_settings_grid_wifi_menu_set_device(QuickSettingsGridWifiMenu *grid,
                                              NMDeviceWifi *dev);

GtkWidget *quick_settings_grid_wifi_menu_get_widget(
    QuickSettingsGridWifiMenu *grid);
