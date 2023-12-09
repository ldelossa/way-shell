#pragma once

#include <adwaita.h>

typedef struct _QuickSettingsBatteryButton {
    GtkBox *container;
    GtkImage *icon;
    GtkLabel *percentage;
    GtkButton *button;
} QuickSettingsBatteryButton;

void quick_settings_battery_button_init(QuickSettingsBatteryButton *button);

void quick_settings_battery_button_free(QuickSettingsBatteryButton *button);
