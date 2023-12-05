#include <adwaita.h>

#include "./panel/panel.h"

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
    global = ADW_APPLICATION_WINDOW(
        adw_application_window_new(GTK_APPLICATION(app)));
    gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(global));

    g_log("main.c: activate()", G_LOG_LEVEL_INFO, "activating application");
    panel_activate(app, user_data);
}

int main(int argc, char *argv[]) {
    AdwApplication *app;
    AdwStyleManager *style_mgr = adw_style_manager_get_default();

    adw_style_manager_set_color_scheme(style_mgr, ADW_COLOR_SCHEME_PREFER_DARK);

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
