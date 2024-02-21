
#include "./quick_settings_grid_idle_inhibitor.h"

#include <adwaita.h>

#include "../../../services/logind_service/logind_service.h"
#include "./quick_settings_grid_button.h"

static void on_toggle_button_clicked(
    GtkButton *button, QuickSettingsGridIdleInhibitorButton *self) {
    g_debug(
        "quick_settings_grid_idle_inhibitor.c: on_toggle_button_clicked(): "
        "toggling idle inhibitor");

    LogindService *logind = logind_service_get_global();

    gboolean inhibit = logind_service_get_idle_inhibit(logind);
    if (!inhibit)
        logind_service_set_idle_inhibit(logind, true);
    else
        logind_service_set_idle_inhibit(logind, false);
}

static void on_idle_inhibit_changed(
    LogindService *logind_service, gboolean inhibited,
    QuickSettingsGridIdleInhibitorButton *self) {
    g_debug(
        "quick_settings_grid_idle_inhibitor.c: on_idle_inhibit_changed(): "
        "idle inhibitor changed: %d",
        inhibited);

    if (inhibited)
        gtk_widget_remove_css_class(GTK_WIDGET(self->button.toggle), "off");
    else
        gtk_widget_add_css_class(GTK_WIDGET(self->button.toggle), "off");
}

QuickSettingsGridIdleInhibitorButton *
quick_settings_grid_idle_inhibitor_button_init() {
    QuickSettingsGridIdleInhibitorButton *self =
        g_malloc0(sizeof(QuickSettingsGridIdleInhibitorButton));

    quick_settings_grid_button_init(
        &self->button, QUICK_SETTINGS_BUTTON_IDLE_INHIBITOR, "Idle Inhibit",
        NULL, "radio-mixed-symbolic", NULL, NULL);

    g_signal_connect(self->button.toggle, "clicked",
                     G_CALLBACK(on_toggle_button_clicked), self);

    // get initial inhibitor state
    LogindService *logind = logind_service_get_global();
    gboolean inhibit = logind_service_get_idle_inhibit(logind);

    on_idle_inhibit_changed(logind, inhibit, self);

    // listen for idle_inhibitor_changed
    g_signal_connect(logind, "idle-inhibitor-changed",
                     G_CALLBACK(on_idle_inhibit_changed), self);

    return self;
}

void quick_settings_grid_idle_inhibitor_button_free(
    QuickSettingsGridIdleInhibitorButton *self) {
    // unref button's container
    g_object_unref(self->button.container);
    // free ourselves
    g_free(self);
}