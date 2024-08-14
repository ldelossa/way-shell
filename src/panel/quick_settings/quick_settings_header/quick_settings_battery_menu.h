#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

struct _QuickSettingsBatteryMenu;
#define QUICK_SETTINGS_BATTERY_MENU_TYPE quick_settings_battery_menu_get_type()
G_DECLARE_FINAL_TYPE(QuickSettingsBatteryMenu, quick_settings_battery_menu,
                     QUICK_SETTINGS, BATTERY_MENU, GObject);

G_END_DECLS

// Called to reinitialize the widget without allocating a new one.
void quick_settings_battery_menu_reinitialize(QuickSettingsBatteryMenu *self);

GtkWidget *quick_settings_battery_menu_get_widget(QuickSettingsBatteryMenu *self);
