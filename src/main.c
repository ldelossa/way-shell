#include <adwaita.h>

#include "./osd/osd.h"
#include "./panel/message_tray/message_tray.h"
#include "./panel/message_tray/message_tray_mediator.h"
#include "./panel/panel.h"
#include "./services/theme_service.h"
#include "./services/brightness_service/brightness_service.h"
#include "./services/clock_service.h"
#include "./services/ipc_service/ipc_service.h"
#include "./services/logind_service/logind_service.h"
#include "./services/network_manager_service.h"
#include "./services/notifications_service/notifications_service.h"
#include "./services/power_profiles_service/power_profiles_service.h"
#include "./services/upower_service.h"
#include "./services/window_manager_service/sway/window_manager_service_sway.h"
#include "./services/wireplumber_service.h"
#include "./services/wayland_service/wayland_service.h"
#include "gresources.h"
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
    GError *error;

    // CSS Theme is embedded as a GResource
    GResource *resource = gresources_get_resource();
    g_resources_register(resource);

    /* GtkCssProvider *provider = gtk_css_provider_new(); */
    /* gtk_css_provider_load_from_resource( */
    /*     provider, "/org/ldelossa/way-shell/data/theme/way-shell-dark.css"); */
    /**/
    /* GdkSeat *seat = gdk_display_get_default_seat(gdk_display_get_default()); */
    /* GdkDisplay *display = gdk_seat_get_display(seat); */
    /* gtk_style_context_add_provider_for_display( */
    /*     display, GTK_STYLE_PROVIDER(provider), */
    /*     GTK_STYLE_PROVIDER_PRIORITY_THEME); */
    /**/

    global = ADW_APPLICATION_WINDOW(
        adw_application_window_new(GTK_APPLICATION(app)));
    gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(global));

    // Service activation //

    g_debug("main.c: activate(): activating services");

    if (clock_service_global_init() != 0) {
        g_error("main.c: activate(): failed to initialize clock service.");
    }

	if (wayland_service_global_init() != 0) {
		g_error("main.c: activate(): failed to initialize wayland service.");
	}

	if (theme_service_global_init() != 0) {
		g_error("main.c: activate(): failed to initialize theme service.");
	}

    if (network_manager_service_global_init() != 0) {
        g_error(
            "main.c: activate(): failed to initialize network manager "
            "service.");
    }

    if (upower_service_global_init() != 0) {
        g_error("main.c: activate(): failed to initialize upower service.");
    }

    if (wire_plumber_service_global_init() != 0) {
        g_error(
            "main.c: activate(): failed to initialize wireplumber "
            "service.");
    }

    if (wm_service_sway_global_init() != 0) {
        g_error("main.c: activate(): failed to initialize sway service.");
    }

    if (notifications_service_global_init() != 0) {
        g_error(
            "main.c: activate(): failed to initialize notifications "
            "service.");
    }

    if (power_profiles_service_global_init() != 0) {
        g_error(
            "main.c: activate(): failed to initialize power profiles "
            "service.");
    }

    if (ipc_service_global_init() != 0) {
        g_error("main.c: activate(): failed to initialize ipc service.");
    }

    if (logind_service_global_init() != 0) {
        g_error("main.c: activate(): failed to initialize logind service.");
    }

    if (brightness_service_global_init() != 0) {
        g_error("main.c: activate(): failed to initialize brightness service.");
    }

    // Subsystem activation //

    g_debug("main.c: activate(): activating subsystems");

    panel_activate(app, user_data);
    g_debug("main.c: activate(): panel subsystems activated");

    message_tray_activate(app, user_data);
    g_debug("main.c: activate(): message_tray subsystems activated");

    quick_settings_activate(app, user_data);
    g_debug("main.c: activate(): quick_settings subsystems activated");

    osd_activate(app, user_data);
    g_debug("main.c: activate(): osd subsystems activated");

    // Subsystem mediator connections //

    g_debug("main.c: activate(): connecting mediator signals");
    panel_mediator_connect(panel_get_global_mediator());
    message_tray_mediator_connect(message_tray_get_global_mediator());
    quick_settings_mediator_connect(quick_settings_get_global_mediator());
}

int main(int argc, char *argv[]) {
    AdwApplication *app;

    // while in beta, just keep this on, once a real versino of this shell is
    // released remove this.
    g_setenv("G_MESSAGES_DEBUG", "all", TRUE);

    int status;

    app = adw_application_new("org.ldelossa.way-shell",
                              G_APPLICATION_DEFAULT_FLAGS);

    gtk_app = app;

    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    status = g_application_run(G_APPLICATION(app), argc, argv);

    g_object_unref(app);

    g_info("main.c: application exited with status: %d", status);
    return status;
}
