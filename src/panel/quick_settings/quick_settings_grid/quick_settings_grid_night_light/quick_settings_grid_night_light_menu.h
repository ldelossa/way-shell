#pragma once

#include <NetworkManager.h>
#include <adwaita.h>

G_BEGIN_DECLS

struct _QuickSettingsGridNightLightMenu;
#define QUICK_SETTINGS_GRID_NIGHT_LIGHT_MENU_TYPE \
    quick_settings_grid_night_light_menu_get_type()
G_DECLARE_FINAL_TYPE(QuickSettingsGridNightLightMenu,
                     quick_settings_grid_night_light_menu, QUICK_SETTINGS,
                     GRID_NIGHT_LIGHT_MENU, GObject);

G_END_DECLS

GtkScale *quick_settings_grid_night_light_menu_get_temp_scale(
    QuickSettingsGridNightLightMenu *grid);

GtkWidget *quick_settings_grid_night_light_menu_get_widget(
    QuickSettingsGridNightLightMenu *grid);
