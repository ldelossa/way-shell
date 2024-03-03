#include "theme_service.h"

#include <adwaita.h>

#include "../gresources.h"
#include "gtk/gtkcssprovider.h"

static ThemeService *global = NULL;

enum signals { theme_changed, signals_n };

struct _ThemeService {
    GObject parent_instance;
    GSettings *setting;
    gboolean light_theme;
    GtkCssProvider *provider;
};
static guint signals[signals_n] = {0};
G_DEFINE_TYPE(ThemeService, theme_service, G_TYPE_OBJECT);

// Stub out GObject's dispose, finalize, class_init, and init methods
static void theme_service_dispose(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(theme_service_parent_class)->dispose(gobject);
};

static void theme_service_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(theme_service_parent_class)->finalize(gobject);
};

static void on_theme_switch(ThemeService *self) {
    const char *conf_dir = g_get_user_config_dir();
    char *arg = NULL;
    if (theme_service_get_theme(self) == THEME_LIGHT) {
        arg = "light";
    } else {
        arg = "dark";
    }

    // check if {config_dir}/way-shell/on_theme_changed.sh file exists
    // if it does, execute it with
    char *script_path =
        g_build_filename(conf_dir, "way-shell", "on_theme_changed.sh", NULL);
    if (g_file_test(script_path,
                    G_FILE_TEST_EXISTS | G_FILE_TEST_IS_EXECUTABLE)) {
        char *cmd = g_strdup_printf("bash -c '%s %s'", script_path, arg);
        GError *error = NULL;
        g_spawn_command_line_async(cmd, &error);
        if (error != NULL) {
            g_warning("Failed to execute command: %s", error->message);
            g_error_free(error);
        }
    }
}

static void theme_service_class_init(ThemeServiceClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = theme_service_dispose;
    object_class->finalize = theme_service_finalize;

    signals[theme_changed] = g_signal_new(
        "theme-changed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0, NULL,
        NULL, NULL, G_TYPE_NONE, 1, G_TYPE_INT);
};

void theme_service_set_light_theme(ThemeService *self, gboolean light_theme) {
    g_debug(
        "theme_service.c:theme_service_set_light_theme(): setting light theme");

    GResource *resource = gresources_get_resource();
    g_resources_register(resource);
    gtk_css_provider_load_from_resource(
        self->provider,
        "/org/ldelossa/way-shell/data/theme/way-shell-light.css");
    g_signal_emit_by_name(self, "theme-changed", THEME_LIGHT);

    GdkSeat *seat = gdk_display_get_default_seat(gdk_display_get_default());
    GdkDisplay *display = gdk_seat_get_display(seat);
    gtk_style_context_add_provider_for_display(
        display, GTK_STYLE_PROVIDER(self->provider),
        GTK_STYLE_PROVIDER_PRIORITY_THEME);

    g_settings_set_boolean(self->setting, "light-theme", true);
    on_theme_switch(self);
};

void theme_service_set_dark_theme(ThemeService *self, gboolean dark_theme) {
    g_debug(
        "theme_service.c:theme_service_set_dark_theme(): setting dark theme");

    GResource *resource = gresources_get_resource();
    g_resources_register(resource);
    gtk_css_provider_load_from_resource(
        self->provider,
        "/org/ldelossa/way-shell/data/theme/way-shell-dark.css");
    g_signal_emit_by_name(self, "theme-changed", THEME_DARK);

    GdkSeat *seat = gdk_display_get_default_seat(gdk_display_get_default());
    GdkDisplay *display = gdk_seat_get_display(seat);
    gtk_style_context_add_provider_for_display(
        display, GTK_STYLE_PROVIDER(self->provider),
        GTK_STYLE_PROVIDER_PRIORITY_THEME);

    g_settings_set_boolean(self->setting, "light-theme", false);
    on_theme_switch(self);
};

static void on_settings_changed(GSettings *settings, const gchar *key,
                                gpointer user_data) {
    g_debug("theme_service.c:on_settings_changed(): settings changed");

    ThemeService *self = THEME_SERVICE(user_data);
    gboolean light_theme = g_settings_get_boolean(self->setting, key);
    if (light_theme) {
        theme_service_set_light_theme(self, light_theme);
    } else {
        theme_service_set_dark_theme(self, light_theme);
    }
};

enum ThemeServiceTheme theme_service_get_theme(ThemeService *self) {
    gboolean light_theme = g_settings_get_boolean(self->setting, "light-theme");
    if (light_theme) {
        return THEME_LIGHT;
    }
    return THEME_DARK;
}

static void theme_service_init(ThemeService *self) {
    self->setting = g_settings_new("org.ldelossa.way-shell.system");

    self->provider = gtk_css_provider_new();
    g_object_ref(self->provider);

    on_settings_changed(self->setting, "light-theme", self);

    g_signal_connect(self->setting, "changed::light-theme",
                     G_CALLBACK(on_settings_changed), self);
};

int theme_service_global_init(void) {
    global = g_object_new(THEME_SERVICE_TYPE, NULL);
    return 0;
}

ThemeService *theme_service_get_global() {
    if (global == NULL) {
        global = g_object_new(THEME_SERVICE_TYPE, NULL);
    }
    return global;
};
