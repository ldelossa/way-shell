#include "quick_settings_grid_oneshot_button.h"

#include <adwaita.h>

#include "quick_settings_grid_button.h"

static void on_clicked(GtkButton *button, QuickSettingsGridButton *self) {
    if (self->cluster)
        // emit cluster remove button req to remove ourselves.
        g_signal_emit_by_name(self->cluster, "remove_button_req", self);
}

void quick_settings_grid_oneshot_button_init_layout(
    QuickSettingsGridOneShotButton *self, const char *title,
    const char *subtitle, const char *icon) {
    quick_settings_grid_button_init(&self->button,
                                    QUICK_SETTINGS_BUTTON_ONESHOT, title,
                                    subtitle, icon, false);

    // wire button click
    g_signal_connect(self->button.toggle, "clicked", G_CALLBACK(on_clicked),
                     self);
}

QuickSettingsGridOneShotButton *quick_settings_grid_oneshot_button_init(
    const char *title, const char *subtitle, const char *icon) {
    QuickSettingsGridOneShotButton *self =
        g_malloc0(sizeof(QuickSettingsGridOneShotButton));
    quick_settings_grid_oneshot_button_init_layout(self, title, subtitle, icon);
    return self;
}

void quick_settings_grid_oneshot_button_free(
    QuickSettingsGridOneShotButton *self);
