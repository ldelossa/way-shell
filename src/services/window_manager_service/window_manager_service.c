#include "window_manager_service.h"

#include "./sway/window_manager_service_sway.h"

static WindowManager *global = NULL;

char *WMWorkspaceEventStringTbl[WMWORKSPACE_EVENT_LEN] = {
    "CREATED", "DESTROYED", "FOCUSED", "MOVED", "RENAMED", "URGENT", "RELOAD",
};

// Initialize the window manager service
int window_manager_service_init() {
    GSettings *settings =
        g_settings_new("org.ldelossa.way-shell.window-manager");

    char *backend = g_settings_get_string(settings, "backend");

    if (g_strcmp0(backend, "sway") == 0) {
        global = wm_service_sway_window_manager_init();
    } else {
        g_warning("Unknown backend: %s", backend);
        return -1;
    }
    return 0;
}

// Obtain the global window manager service, a call to
// `window_manager_service_init` must be made before this.
WindowManager *window_manager_service_get_global() { return global; }
