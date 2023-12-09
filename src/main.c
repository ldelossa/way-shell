#include <adwaita.h>

#include "./panel/message_tray/message_tray.h"
#include "./panel/message_tray/message_tray_mediator.h"
#include "./panel/panel.h"
#include "./services/clock_service.h"
#include "./services/upower_service.h"
#include "./services/wireplumber_service.h"
#include "panel/panel_mediator.h"
#include "panel/quick_settings/quick_settings.h"

// The global GTK application.
// This can be down casted to a AdwApplication if necessary.
// Decoupled code can access this variable with `extern` storage class.
AdwApplication *gtk_app = NULL;

// Global application window which stays hidden.
// This ensures our app does not exit if all monitors are removed from the
// system along with the other responsibilities of a ApplicationWindow.
AdwApplicationWindow *global = NULL;

// activates all the components of our shell.
static void activate(AdwApplication *app, gpointer user_data) {
    // set CSS style sheet for application
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(
        provider,
        "/home/louis/git/c/gnomeland/data/theme/gnome-shell-dark.css");

    GdkSeat *seat = gdk_display_get_default_seat(gdk_display_get_default());
    GdkDisplay *display = gdk_seat_get_display(seat);
    gtk_style_context_add_provider_for_display(
        display, GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_THEME);

    global = ADW_APPLICATION_WINDOW(
        adw_application_window_new(GTK_APPLICATION(app)));
    gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(global));

    g_log("main.c: activate()", G_LOG_LEVEL_INFO, "activating services");
    if (clock_service_global_init() != 0) {
        g_error("main.c: activate(): failed to initialize clock service.");
    }
    g_debug("main.c: activate(): clock service initialized.");
    if (upower_service_global_init() != 0) {
        g_error("main.c: activate(): failed to initialize upower service.");
    }
    g_debug("main.c: activate(): upower service initialized.");
    if (wire_plumber_service_global_init() != 0) {
        g_error("main.c: activate(): failed to initialize wireplumber "
                "service.");
    }

    g_log("main.c: activate()", G_LOG_LEVEL_INFO, "activating subsystems");
    panel_activate(app, user_data);
    g_log("main.c: activate()", G_LOG_LEVEL_INFO, "panel subsystems activated");
    message_tray_activate(app, user_data);
    g_log("main.c: activate()", G_LOG_LEVEL_INFO,
          "message_tray subsystems activated");
    quick_settings_activate(app, user_data);
    g_log("main.c: activate()", G_LOG_LEVEL_INFO,
          "quick_settings subsystems activated");

    g_log("main.c: activate()", G_LOG_LEVEL_INFO,
          "connecting mediator signals");
    panel_mediator_connect(panel_get_global_mediator());
    message_tray_mediator_connect(message_tray_get_global_mediator());
    quick_settings_mediator_connect(quick_settings_get_global_mediator());
}

int main(int argc, char *argv[]) {
    AdwApplication *app;
    // AdwStyleManager *style_mgr = adw_style_manager_get_default();
    // adw_style_manager_set_color_scheme(style_mgr,
    // ADW_COLOR_SCHEME_PREFER_DARK);

    // TODO: make this configurable.
    g_setenv("G_MESSAGES_DEBUG", "all", TRUE);

    int status;

    app = adw_application_new("org.ldelossa.gnomeland",
                              G_APPLICATION_DEFAULT_FLAGS);

    gtk_app = app;

    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    status = g_application_run(G_APPLICATION(app), argc, argv);

    g_object_unref(app);

    g_info("main.c: application exited with status: %d", status);
    return status;
}
