#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

struct _QuickSettingsPowerMenu;
#define QUICK_SETTINGS_POWER_MENU_TYPE quick_settings_power_menu_get_type()
G_DECLARE_FINAL_TYPE(QuickSettingsPowerMenu, quick_settings_power_menu,
                     QUICK_SETTINGS, POWER_MENU, GObject);

G_END_DECLS

// Called to reinitialize the widget without allocating a new one.
void quick_settings_power_menu_reinitialize(QuickSettingsPowerMenu *self);

GtkWidget *quick_settings_power_menu_get_widget(QuickSettingsPowerMenu *self);
