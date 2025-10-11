#include "window_manager_service_niri.h"

#include <adwaita.h>
#include <glib-2.0/glib-unix.h>
#include <sys/socket.h>
#include <unistd.h>

#include "glib-object.h"
#include "glib.h"
#include "niri_client.h"

enum signals {
  workspaces_changed,
  outputs_changed, // NOTE: niri doesn't have output events: https://docs.rs/niri-ipc/25.8.0/niri_ipc/enum.Event.html
  signals_n
};

struct _WMServiceNiri {
    GObject parent_instance;
    GPtrArray *workspaces;
    GPtrArray *outputs;
    char *socket_path;
    GIOChannel *event_channel;
    GIOChannel *command_channel;
    guint poll_id;
    gboolean subscribed;
    gchar *focused_workspace;
    GSettings *settings;
};

static guint service_signals[signals_n] = {0};
G_DEFINE_TYPE(WMServiceNiri, wm_service_niri, G_TYPE_OBJECT);

static void wm_service_niri_dispose(GObject *gobject) {
    WMServiceNiri *self = WM_SERVICE_NIRI(gobject);

    // close sockets
    g_io_channel_unref(self->event_channel);
    g_io_channel_unref(self->command_channel);

    // g_free socket path
    g_free(self->socket_path);

    if (self->workspaces) g_ptr_array_unref(self->workspaces);
    if (self->outputs) g_ptr_array_unref(self->outputs);

    // Chain-up
    G_OBJECT_CLASS(wm_service_niri_parent_class)->dispose(gobject);
};

static void wm_service_niri_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(wm_service_niri_parent_class)->finalize(gobject);
};

static void wm_service_niri_class_init(WMServiceNiriClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = wm_service_niri_dispose;
    object_class->finalize = wm_service_niri_finalize;

    service_signals[workspaces_changed] = g_signal_new(
        "workspaces-changed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_PTR_ARRAY);

    // NOTE: doesn't actually do anything, niri doesn't have output events
    service_signals[outputs_changed] = g_signal_new(
        "outputs-changed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_PTR_ARRAY);
}

static void wm_service_niri_init(WMServiceNiri *self) {
    // initialize only sockets here i.e. niri specific stuff
    // generic stuff is initialized in wm_service_niri_window_manager_init
    self->socket_path = niri_client_find_socket_path();
    if (!self->socket_path)
        g_error(
            "window_manager_service_niri.c:wm_service_niri_init "
            "failed to find socket path.");

    g_debug(
        "window_manager_service_niri.c:wm_service_niri_init "
        "found socket path: %s",
        self->socket_path);
    self->event_channel = niri_client_connect(self->socket_path);
    if (self->event_channel == 0)
        g_error(
            "window_manager_service_niri.c:wm_service_niri_init "
            "failed to connect to event socket.");

    self->command_channel = niri_client_connect(self->socket_path);
    if (self->command_channel == 0)
        g_error(
            "window_manager_service_niri.c:wm_service_niri_init "
            "failed to connect to command socket.");

    self->poll_id = 0;
    self->subscribed = FALSE;

    // connect to 'org.ldelossa.way-shell.window-manager.workspaces' setting
    self->settings = g_settings_new("org.ldelossa.way-shell.window-manager");

    self->workspaces = NULL;
    self->outputs = NULL;
    self->focused_workspace = NULL;
}

static gboolean on_ipc_recv(GIOChannel *channel, GIOCondition condition, WMServiceNiri *self) {
    g_debug(
        "window_manager_service_niri.c:on_ipc_recv() "
        "received ipc message.");

    // TODO: implement recovery from this.
    if (condition & G_IO_HUP || condition & G_IO_ERR) {
        g_debug(
            "window_manager_service_niri.c:handle_ipc_recv() "
            "received G_IO_HUP or G_IO_ERR, GLib polling stopped.");
        return false;
    }

    if (condition & G_IO_IN) {
        gchar *line = nullptr;
        gsize length = 0;
        gsize terminator_pos = -1;
        GError *error = nullptr;
        GIOStatus status = g_io_channel_read_line(channel, &line, &length, &terminator_pos, &error);

        if (status == G_IO_STATUS_NORMAL) {
            g_debug("window_manager_service_niri.c:on_socket_ready() received event: %s", line);

            // Check for workspace-related events
            if (g_strstr_len(line, -1, "WorkspacesChanged") != NULL ||
                g_strstr_len(line, -1, "WorkspaceActivated") != NULL ||
                g_strstr_len(line, -1, "WorkspaceUrgencyChanged") != NULL ||
                g_strstr_len(line, -1, "WorkspaceActiveWindowChanged") != NULL) {

                // Refresh workspace list on any workspace event
                g_debug("window_manager_service_niri.c:on_socket_ready() refreshing workspaces due to workspace event");
                if (self->workspaces) g_ptr_array_unref(self->workspaces);
                self->workspaces = niri_client_get_workspaces(self->command_channel);

                if (self->workspaces) {
                    g_signal_emit(self, service_signals[workspaces_changed], 0, self->workspaces);
                }
            }
            g_free(line);
        } else {
            // TODO: handle other conditions
        }

    }

    return true;
}

static void wm_service_niri_setup_polling(WMServiceNiri *self) {
    g_debug("window_manager_service_niri.c:wm_service_niri_setup_polling() called");

    // Subscribe to events
    if (!self->subscribed) {
        if (niri_client_subscribe_events(self->event_channel) == 0) {
            self->subscribed = TRUE;
        } else {
            g_warning("window_manager_service_niri.c:wm_service_niri_setup_polling() failed to subscribe to events");
            return;
        }
    }

    // add our connected channel to GLib event loop.
    self->poll_id = g_io_add_watch(self->event_channel, G_IO_IN | G_IO_ERR | G_IO_HUP, (GIOFunc)on_ipc_recv, self);
}

// WindowManager interface implementation
GPtrArray *wm_service_niri_get_workspaces(WindowManager *wm) {
    WMServiceNiri *self = wm->private;

    if (!self->workspaces) {
        g_warning(
            "window_manager_service_niri.c:wm_service_niri_get_workspaces() "
            "workspaces not initialized.");
        return NULL;
    }

    return g_ptr_array_ref(self->workspaces);
}

GPtrArray *wm_service_niri_get_outputs(WindowManager *wm) {
    WMServiceNiri *self = wm->private;

    if (!self->outputs) {
        g_warning(
            "window_manager_service_niri.c:wm_service_niri_get_outputs() "
            "outputs not initialized.");
        return NULL;
    }

    return g_ptr_array_ref(self->outputs);
}

int wm_service_niri_focus_workspace(WindowManager *wm, WMWorkspace *ws) {
    WMServiceNiri *self = wm->private;

    if (!self->workspaces) {
        g_warning(
            "window_manager_service_niri.c:wm_service_niri_focus_workspace() "
            "workspaces not initialized.");
        return -1;
    }
    return niri_client_focus_workspace(self->command_channel, ws->name);
}

int wm_service_niri_rename_current_workspace(WindowManager *wm, const gchar *name) {
    WMServiceNiri *self = wm->private;

    if (strlen(name) == 0) return -1;

    return niri_client_rename_current_workspace(self->command_channel, name);
}

int wm_service_niri_current_ws_to_output(WindowManager *wm, WMOutput *o) {
    WMServiceNiri *self = wm->private;

    if (!o) {
        g_warning(
            "window_manager_service_niri.c:wm_service_niri_current_ws_to_"
            "output() "
            "outputs not initialized.");
        return -1;
    }
    return niri_client_move_workspace_to_output(self->command_channel, o->name);
}

int wm_service_niri_current_app_to_workspace(WindowManager *wm, WMWorkspace *ws) {
    WMServiceNiri *self = wm->private;

    if (!ws) {
        g_warning(
            "window_manager_service_niri.c:wm_service_niri_current_app_to_"
            "workspace() "
            "workspaces not initialized.");
        return -1;
    }
    return niri_client_move_window_to_workspace(self->command_channel, ws->name);
}

guint wm_service_niri_register_on_workspaces_changed(WindowManager *wm, wm_on_workspaces_changed cb, void *data) {
    WMServiceNiri *self = wm->private;

    // we use swapped here because workspaces_changed functions should not
    // leak the private workspace service's implementation in their signatures.
    return g_signal_connect_swapped(self, "workspaces-changed", G_CALLBACK(cb),
                                    data);
}

guint wm_service_niri_unregister_on_workspaces_changed(WindowManager *wm, wm_on_workspaces_changed cb, void *data) {
    WMServiceNiri *self = wm->private;

    return g_signal_handlers_disconnect_by_func(self, cb, data);
}

// NOTE: doesn't actually do anything, niri doesn't have events for outputs
guint wm_service_niri_register_on_outputs_changed(WindowManager *wm, wm_on_outputs_changed cb, void *data) {
    WMServiceNiri *self = wm->private;

    // we use swapped here because workspaces_changed functions should not
    // leak the private workspace service's implementation in their signatures.
    return g_signal_connect_swapped(self, "outputs-changed", G_CALLBACK(cb),
                                    data);
}

// NOTE: doesn't actually do anything, niri doesn't have events for outputs
guint wm_service_niri_unregister_on_outputs_changed(WindowManager *wm, wm_on_outputs_changed cb, void *data) {
    WMServiceNiri *self = wm->private;

    return g_signal_handlers_disconnect_by_func(self, cb, data);
}

WindowManager *wm_service_niri_window_manager_init() {
    WindowManager *wm = g_malloc(sizeof(WindowManager));

    WMServiceNiri  *self = g_object_new(WM_SERVICE_NIRI_TYPE, NULL);

    // write virt func table
    wm->private = self;
    wm->get_workspaces = wm_service_niri_get_workspaces;
    wm->get_outputs = wm_service_niri_get_outputs;
    wm->focus_workspace = wm_service_niri_focus_workspace;
    wm->rename_workspace = wm_service_niri_rename_current_workspace;
    wm->current_ws_to_output = wm_service_niri_current_ws_to_output;
    wm->current_app_to_workspace = wm_service_niri_current_app_to_workspace;
    wm->register_on_workspaces_changed = wm_service_niri_register_on_workspaces_changed;
    wm->unregister_on_workspaces_changed = wm_service_niri_unregister_on_workspaces_changed;
    wm->register_on_outputs_changed = wm_service_niri_register_on_outputs_changed;
    wm->unregister_on_outputs_changed = wm_service_niri_unregister_on_outputs_changed;

    // subscribe to events
    wm_service_niri_setup_polling(self);

    // get initial listing of workspaces
    self->workspaces = niri_client_get_workspaces(self->command_channel);

    // get initial listing of outputs
    self->outputs = niri_client_get_outputs(self->command_channel);

    return wm;
}
