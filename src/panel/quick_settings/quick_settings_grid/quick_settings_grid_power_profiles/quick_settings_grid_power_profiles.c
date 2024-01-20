#include "./quick_settings_grid_power_profiles.h"

#include <adwaita.h>

#include "../../../../services/power_profiles_service/power_profiles_service.h"
#include "./../quick_settings_grid_button.h"

static void on_active_profile_change(
    PowerProfilesService *pps, char *power_profile,
    QuickSettingsGridPowerProfilesButton *self) {
    gtk_label_set_text(self->button.subtitle, power_profile);

    const char *icon = power_profiles_service_profile_to_icon(power_profile);
    gtk_image_set_from_icon_name(self->button.icon, icon);
}

void quick_settings_grid_power_profiles_button_layout(
    QuickSettingsGridPowerProfilesButton *self) {
    // get power profiles service
    PowerProfilesService *power_profiles_service =
        power_profiles_service_get_global();

    // get active profile for subtitle and icon
    const gchar *active_profile =
        power_profiles_service_get_active_profile(power_profiles_service);

    const char *icon = power_profiles_service_profile_to_icon(active_profile);
    quick_settings_grid_button_init(&self->button,
                                    QUICK_SETTINGS_BUTTON_PERFORMANCE,
                                    "Performance", active_profile, icon, true);

    // attach to active profile changed event
    g_signal_connect(power_profiles_service, "active-profile-changed",
                     G_CALLBACK(on_active_profile_change), self);
}

QuickSettingsGridPowerProfilesButton *
quick_settings_grid_power_profiles_button_init() {
    QuickSettingsGridPowerProfilesButton *self =
        g_malloc0(sizeof(QuickSettingsGridPowerProfilesButton));

    self->menu =
        g_object_new(QUICK_SETTINGS_GRID_POWER_PROFILES_MENU_TYPE, NULL);

    quick_settings_grid_power_profiles_button_layout(self);

    return self;
}

GtkWidget *quick_settings_grid_power_profiles_button_get_menu_widget(
    QuickSettingsGridPowerProfilesButton *self) {
    return GTK_WIDGET(
        quick_settings_grid_power_profiles_menu_get_widget(self->menu));
}

void quick_settings_grid_power_profiles_button_free(
    QuickSettingsGridPowerProfilesButton *self) {
    g_debug(
        "quick_settings_grid_power_profiles.c:qs_grid_power_profiles_button_"
        "free() called");

    // unref our menu
    g_object_unref(self->menu);
    // unref button's container
    g_object_unref(self->button.container);
    // free ourselves
    g_free(self);
}