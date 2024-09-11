#include "./quick_settings_grid_night_light.h"

#include <adwaita.h>

#include "../../../../services/wayland/gamma_control_service/gamma.h"

void quick_settings_grid_night_light_button_layout(
    QuickSettingsGridNightLightButton *self) {
    // create associated menu
    self->menu = g_object_new(QUICK_SETTINGS_GRID_NIGHT_LIGHT_MENU_TYPE, NULL);

    quick_settings_grid_button_init(
        &self->button, QUICK_SETTINGS_BUTTON_NIGHT_LIGHT, "Night Light", NULL,
        "night-light-symbolic",
        quick_settings_grid_night_light_button_get_menu_widget(self),
        quick_settings_grid_night_light_menu_on_reveal);
}

static void on_gamma_control_enabled(WaylandGammaControlService *gamma,
                                     QuickSettingsGridNightLightButton *self) {
    quick_settings_grid_button_set_toggled(&self->button, true);
}

static void on_gamma_control_disabled(WaylandGammaControlService *gamma,
                                      QuickSettingsGridNightLightButton *self) {
    quick_settings_grid_button_set_toggled(&self->button, false);
}

static void on_toggle_button_clicked(GtkButton *button,
                                     QuickSettingsGridNightLightButton *self) {
    g_debug(
        "quick_settings_grid_night_light.c: on_toggle_button_clicked(): "
        "toggling night light");

    GtkScale *temp_scale =
        quick_settings_grid_night_light_menu_get_temp_scale(self->menu);

    WaylandGammaControlService *w = wayland_gamma_control_service_get_global();

    gboolean enabled = wayland_gamma_control_service_enabled(w);
    if (!enabled) {
        wayland_gamma_control_service_set_temperature(
            w, gtk_range_get_value(GTK_RANGE(temp_scale)));
    } else {
        // reset scale value to 3000k
        gtk_range_set_value(GTK_RANGE(temp_scale), 3000);
        wayland_gamma_control_service_destroy(w);
    }
}

QuickSettingsGridNightLightButton *
quick_settings_grid_night_light_button_init() {
    QuickSettingsGridNightLightButton *self =
        g_malloc0(sizeof(QuickSettingsGridNightLightButton));

    quick_settings_grid_night_light_button_layout(self);

    WaylandGammaControlService *w = wayland_gamma_control_service_get_global();
    g_signal_connect(w, "gamma-control-enabled",
                     G_CALLBACK(on_gamma_control_enabled), self);
    g_signal_connect(w, "gamma-control-disabled",
                     G_CALLBACK(on_gamma_control_disabled), self);

    gboolean enabled = wayland_gamma_control_service_enabled(w);
    if (enabled) {
        quick_settings_grid_button_set_toggled(&self->button, true);
    } else {
        quick_settings_grid_button_set_toggled(&self->button, false);
    }

    g_signal_connect(self->button.toggle, "clicked",
                     G_CALLBACK(on_toggle_button_clicked), self);

    return self;
}

GtkWidget *quick_settings_grid_night_light_button_get_menu_widget(
    QuickSettingsGridNightLightButton *self) {
    return GTK_WIDGET(
        quick_settings_grid_night_light_menu_get_widget(self->menu));
}

void quick_settings_grid_night_light_button_free(
    QuickSettingsGridNightLightButton *self) {
    g_debug(
        "quick_settings_grid_night_light.c:qs_grid_night_light_button_free() "
        "called");

    // kill gamma control signals
    WaylandGammaControlService *w = wayland_gamma_control_service_get_global();
    g_signal_handlers_disconnect_by_data(w, self);

    // free ourselves
    g_free(self);
}
