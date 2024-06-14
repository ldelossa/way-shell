#pragma once

#include <NetworkManager.h>
#include <adwaita.h>

G_BEGIN_DECLS

struct _QuickSettingsGridKeyboardBrightnessMenu;
#define QUICK_SETTINGS_GRID_KEYBOARD_BRIGHTNESS_MENU_TYPE \
    quick_settings_grid_keyboard_brightness_menu_get_type()
G_DECLARE_FINAL_TYPE(QuickSettingsGridKeyboardBrightnessMenu,
                     quick_settings_grid_keyboard_brightness_menu, QUICK_SETTINGS,
                     GRID_KEYBOARD_BRIGHTNESS_MENU, GObject);

G_END_DECLS

GtkScale *quick_settings_grid_keyboard_brightness_menu_get_temp_scale(
    QuickSettingsGridKeyboardBrightnessMenu *grid);

GtkWidget *quick_settings_grid_keyboard_brightness_menu_get_widget(
    QuickSettingsGridKeyboardBrightnessMenu *grid);
