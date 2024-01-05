#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

struct _QuickSettingsBatteryButton;
#define QUICK_SETTINGS_BATTERY_BUTTON_TYPE \
    quick_settings_battery_button_get_type()
G_DECLARE_FINAL_TYPE(QuickSettingsBatteryButton, quick_settings_battery_button,
                     QUICK_SETTINGS, BATTERY_BUTTON, GObject);

G_END_DECLS

void quick_settings_battery_button_reinitialize(
    QuickSettingsBatteryButton *self);

GtkWidget *quick_settings_battery_button_get_widget(
    QuickSettingsBatteryButton *self);
