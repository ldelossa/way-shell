#include "./quick_settings_grid_keyboard_brightness.h"

#include <adwaita.h>

#include "../../../../services/brightness_service/brightness_service.h"
#include "glib-object.h"

void quick_settings_grid_keyboard_brightness_button_layout(
    QuickSettingsGridKeyboardBrightnessButton *self) {
    // create associated menu
    self->menu =
        g_object_new(QUICK_SETTINGS_GRID_KEYBOARD_BRIGHTNESS_MENU_TYPE, NULL);

    quick_settings_grid_button_init(
        &self->button, QUICK_SETTINGS_BUTTON_KEYBOARD_BRIGHTNESS, "Keyboard",
        NULL, "keyboard-brightness-symbolic",
        quick_settings_grid_keyboard_brightness_button_get_menu_widget(self),
        quick_settings_grid_keyboard_brightness_menu_on_reveal);
}

static void on_toggle_button_clicked(
    GtkButton *button, QuickSettingsGridKeyboardBrightnessButton *self) {
    g_debug(
        "quick_settings_grid_keyboard_brightness.c: "
        "on_toggle_button_clicked(): "
        "toggling keyboard backlight");

    BrightnessService *bs = brightness_service_get_global();
    guint keyboard_brightness = brightness_service_get_keyboard(bs);

    if (keyboard_brightness > 0)
        brightness_service_set_keyboard(bs, 0);
    else
        brightness_service_set_keyboard(bs, 1);
}

static void on_keyboard_brightness_changed(
    BrightnessService *bs, guint keyboard_brightness,
    QuickSettingsGridKeyboardBrightnessButton *self) {
    g_debug(
        "quick_settings_grid_keyboard_brightness.c: "
        "on_keyboard_brightness_changed(): "
        "keyboard_brightness = %d",
        keyboard_brightness);

    if (keyboard_brightness > 0)
        quick_settings_grid_button_set_toggled(&self->button, TRUE);
    else
        quick_settings_grid_button_set_toggled(&self->button, FALSE);
}

QuickSettingsGridKeyboardBrightnessButton *
quick_settings_grid_keyboard_brightness_button_init() {
    QuickSettingsGridKeyboardBrightnessButton *self =
        g_malloc0(sizeof(QuickSettingsGridKeyboardBrightnessButton));

    quick_settings_grid_keyboard_brightness_button_layout(self);

    BrightnessService *bs = brightness_service_get_global();

    guint keyboard_brightness = brightness_service_get_keyboard(bs);
    on_keyboard_brightness_changed(bs, keyboard_brightness, self);

    g_signal_connect(self->button.toggle, "clicked",
                     G_CALLBACK(on_toggle_button_clicked), self);
    g_signal_connect(bs, "keyboard-brightness-changed",
                     G_CALLBACK(on_keyboard_brightness_changed), self);

    return self;
}

GtkWidget *quick_settings_grid_keyboard_brightness_button_get_menu_widget(
    QuickSettingsGridKeyboardBrightnessButton *self) {
    return GTK_WIDGET(
        quick_settings_grid_keyboard_brightness_menu_get_widget(self->menu));
}

void quick_settings_grid_keyboard_brightness_button_free(
    QuickSettingsGridKeyboardBrightnessButton *self) {
    g_debug(
        "quick_settings_grid_keyboard_brightness.c:qs_grid_keyboard_brightness_"
        "button_free() "
        "called");

    // kill signals
    BrightnessService *bs = brightness_service_get_global();
    g_signal_handlers_disconnect_by_func(bs, on_keyboard_brightness_changed,
                                         self);

    // unref menu
    g_object_unref(self->menu);

    // free ourselves
    g_free(self);
}
