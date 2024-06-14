#pragma once

#include <NetworkManager.h>
#include <adwaita.h>

#include "../quick_settings_grid_button.h"
#include "quick_settings_grid_keyboard_brightness_menu.h"

#define QUICK_SETTINGS_KEYBOARD_BRIGHTNESS_TYPE "keyboard-brightness"

typedef struct _QuickSesstingsGridKeyboardBrightnessButton {
    // embedd this as first argument so we can cast to it.
    QuickSettingsGridButton button;
    QuickSettingsGridKeyboardBrightnessMenu *menu;
    gboolean enabled;

} QuickSettingsGridKeyboardBrightnessButton;

QuickSettingsGridKeyboardBrightnessButton *
quick_settings_grid_keyboard_brightness_button_init();

void quick_settings_grid_keyboard_brightness_button_free(
    QuickSettingsGridKeyboardBrightnessButton *self);

GtkWidget *quick_settings_grid_keyboard_brightness_button_get_menu_widget(
    QuickSettingsGridKeyboardBrightnessButton *self);

void quick_settings_grid_keyboard_brightness_menu_on_reveal(
    QuickSettingsGridButton *button, gboolean is_revealed);
