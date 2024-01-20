#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

struct _QuickSettingsGridPowerProfilesMenu;
#define QUICK_SETTINGS_GRID_POWER_PROFILES_MENU_TYPE \
    quick_settings_grid_power_profiles_menu_get_type()
G_DECLARE_FINAL_TYPE(QuickSettingsGridPowerProfilesMenu, quick_settings_grid_power_profiles_menu,
                     QUICK_SETTINGS, GRID_POWER_PROFILES_MENU, GObject);

G_END_DECLS

GtkWidget *quick_settings_grid_power_profiles_menu_get_widget(
    QuickSettingsGridPowerProfilesMenu *self);