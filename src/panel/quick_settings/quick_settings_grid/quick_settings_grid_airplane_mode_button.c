#include "./quick_settings_grid_airplane_mode_button.h"

#include <adwaita.h>

#include "../../../services/network_manager_service.h"
#include "./quick_settings_grid_button.h"

static void on_toggle_button_clicked(
    GtkButton *button, QuickSettingsGridAirplaneModeButton *self) {
    NetworkManagerService *nm = network_manager_service_get_global();
    gboolean enabled = network_manager_service_get_networking_enabled(nm);

    if (enabled)
        network_manager_service_networking_enable(nm, false);
    else
        network_manager_service_networking_enable(nm, true);
}

static void on_networking_enabled_changed(
    NetworkManagerService *nm, gboolean enabled,
    QuickSettingsGridAirplaneModeButton *self) {
    g_debug(
        "quick_settings_grid_airplane_mode_button.c: "
        "on_networking_enabled_changed(): "
        "networking enabled changed: %d",
        enabled);

    if (enabled)
        quick_settings_grid_button_set_toggled(&self->button, false);
    else
        quick_settings_grid_button_set_toggled(&self->button, true);
}

QuickSettingsGridAirplaneModeButton *
quick_settings_grid_airplane_mode_button_init() {
    QuickSettingsGridAirplaneModeButton *self =
        g_malloc0(sizeof(QuickSettingsGridAirplaneModeButton));

    quick_settings_grid_button_init(
        &self->button, QUICK_SETTINGS_BUTTON_AIRPLANE_MODE, "Airplane Mode",
        NULL, "airplane-mode-symbolic", NULL, NULL);

    g_signal_connect(self->button.toggle, "clicked",
                     G_CALLBACK(on_toggle_button_clicked), self);

    NetworkManagerService *nm = network_manager_service_get_global();
    gboolean enabled = network_manager_service_get_networking_enabled(nm);

    on_networking_enabled_changed(nm, enabled, self);

    // listen for networking mode changes
    g_signal_connect(nm, "networking-enabled-changed",
                     G_CALLBACK(on_networking_enabled_changed), self);

    return self;
}

void quick_settings_grid_airplane_mode_button_free(
    QuickSettingsGridAirplaneModeButton *self) {
    // unref button's container
    g_object_unref(self->button.container);

    // kill signals
    NetworkManagerService *nm = network_manager_service_get_global();
    g_signal_handlers_disconnect_by_func(nm, on_networking_enabled_changed,
                                         self);

    // free ourselves
    g_free(self);
}
