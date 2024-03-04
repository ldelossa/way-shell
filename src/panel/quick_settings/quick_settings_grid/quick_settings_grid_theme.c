#include "quick_settings_grid_theme.h"

#include <adwaita.h>

#include "../../../services/theme_service.h"
#include "quick_settings_grid_button.h"

static void on_clicked(GtkButton *button, QuickSettingsGridButton *self) {
    ThemeService *ts = theme_service_get_global();
    enum ThemeServiceTheme theme = theme_service_get_theme(ts);
    if (theme == THEME_LIGHT) {
        theme_service_set_dark_theme(ts, TRUE);
    } else {
        theme_service_set_light_theme(ts, TRUE);
    }
}

static void on_theme_changed(ThemeService *ts, enum ThemeServiceTheme theme,
                             QuickSettingsGridButton *self) {
    if (theme == THEME_LIGHT) {
        gtk_label_set_text(self->title, "Dark Style");
    } else {
        gtk_label_set_text(self->title, "Light Style");
    }
}

void quick_settings_grid_theme_button_init_layout(
    QuickSettingsGridOneThemeButton *self) {
    quick_settings_grid_button_init(
        &self->button, QUICK_SETTINGS_BUTTON_THEME, "Dark Style", NULL,
        "preferences-desktop-appearance-symbolic", NULL, NULL);

    ThemeService *ts = theme_service_get_global();
    enum ThemeServiceTheme theme = theme_service_get_theme(ts);
    on_theme_changed(ts, theme, &self->button);

    g_signal_connect(ts, "theme-changed", G_CALLBACK(on_theme_changed),
                     &self->button);

    // wire button click
    g_signal_connect(self->button.toggle, "clicked", G_CALLBACK(on_clicked),
                     self);
}

QuickSettingsGridOneThemeButton *quick_settings_grid_theme_button_init() {
    QuickSettingsGridOneThemeButton *self =
        g_malloc0(sizeof(QuickSettingsGridOneThemeButton));
    quick_settings_grid_theme_button_init_layout(self);
    return self;
}

void quick_settings_grid_theme_button_free(
    QuickSettingsGridOneThemeButton *self) {
    g_signal_handlers_disconnect_by_func(theme_service_get_global(),
                                         on_theme_changed, self);
}
