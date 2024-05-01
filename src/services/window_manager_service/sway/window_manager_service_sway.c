#include "window_manager_service_sway.h"

#include <adwaita.h>
#include <glib-2.0/glib-unix.h>

#include "glib.h"
#include "ipc.h"
#include "sway_client.h"

static WMServiceSway *global = NULL;

enum signals { workspaces_changed, outputs_changed, signals_n };

struct _WMServiceSway {
    GObject parent_instance;
    GPtrArray *workspaces;
    GPtrArray *outputs;
    char *socket_path;
    int socket_fd;
    guint poll_id;
    gboolean polling;
    gboolean subscribed;
    gchar *focused_workspace;
    GSettings *settings;
};
static guint service_signals[signals_n] = {0};
G_DEFINE_TYPE(WMServiceSway, wm_service_sway, G_TYPE_OBJECT);

static void wm_service_sway_dispose(GObject *gobject) {
    WMServiceSway *self = WM_SERVICE_SWAY(gobject);

    // close socket
    close(self->socket_fd);

    // g_free socket path
    g_free(self->socket_path);

    if (self->workspaces) g_ptr_array_unref(self->workspaces);

    // Chain-up
    G_OBJECT_CLASS(wm_service_sway_parent_class)->dispose(gobject);
};

static void wm_service_sway_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(wm_service_sway_parent_class)->finalize(gobject);
};

static void wm_service_sway_class_init(WMServiceSwayClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = wm_service_sway_dispose;
    object_class->finalize = wm_service_sway_finalize;

    service_signals[workspaces_changed] = g_signal_new(
        "workspaces-changed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_PTR_ARRAY);

    service_signals[outputs_changed] = g_signal_new(
        "outputs-changed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0, NULL,
        NULL, NULL, G_TYPE_NONE, 1, G_TYPE_PTR_ARRAY);
};

static gint compare_workspace_name(WMWorkspace **a, WMWorkspace **b) {
    return g_strcmp0((*a)->name, (*b)->name);
}

static void handle_ipc_get_workspaces(WMServiceSway *self,
                                      sway_client_ipc_msg *msg) {
    GPtrArray *tmp = sway_client_ipc_get_workspaces_resp(msg);

    g_debug(
        "window_manager_service_sway.c:handle_ipc_get_workspaces() "
        "called");

    if (!tmp) return;

    if (self->workspaces) {
        g_ptr_array_unref(self->workspaces);
    }
    self->workspaces = tmp;

    // check 'sort-alphabetical' setting and if true sort
    if (g_settings_get_boolean(self->settings, "sort-workspaces-alphabetical"))
        g_ptr_array_sort(self->workspaces,
                         (GCompareFunc)compare_workspace_name);

    // emit signal
    g_signal_emit(self, service_signals[workspaces_changed], 0,
                  self->workspaces);
}

static void handle_ipc_get_outputs(WMServiceSway *self,
                                   sway_client_ipc_msg *msg) {
    GPtrArray *tmp = sway_client_ipc_get_outputs_resp(msg);

    g_debug(
        "window_manager_service_sway.c:handle_ipc_get_workspaces() "
        "called");

    if (!tmp) return;

    if (self->outputs) {
        g_ptr_array_unref(self->outputs);
    }
    self->outputs = tmp;

    // emit signal
    g_signal_emit(self, service_signals[outputs_changed], 0, self->outputs);
}

static void launch_on_workspace_new_script(gchar *name) {
    // determine if XDG_CONFIG_DIR/way-shell/on_workspace_new.sh file exists
    // if it does, execute it with the workspace name as the first argument.
    gchar *path = g_build_filename(g_get_user_config_dir(), "way-shell",
                                   "on_workspace_new.sh", NULL);
    if (!g_file_test(path,
                     G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_EXECUTABLE)) {
        g_free(path);
        return;
    }

    g_spawn_command_line_async(g_strdup_printf("%s %s", path, name), NULL);
    g_free(path);
}

static void handle_ipc_event_workspaces(WMServiceSway *self,
                                        sway_client_ipc_msg *msg) {
    // perform get workspaces req to get latest workspace's state.
    // this maybe slower (another round tip) but works way smoother
    // on spam events like monitor create and deletes.
    g_debug(
        "window_manager_service_sway.c:handle_ipc_event_workspaces() "
        "received workspace event, getting latest workspace listing.");

    // we still want to determine if this is a new workspace and if it is
    // run our "on_workspace_new.sh" script.
    WMWorkspaceEvent *event = sway_client_ipc_event_workspace_resp(msg);
    if (event->type == WMWORKSPACE_EVENT_CREATED) {
        launch_on_workspace_new_script(event->workspace.name);
    }

    if (event->type == WMWORKSPACE_EVENT_FOCUSED) {
        if (self->focused_workspace) g_free(self->focused_workspace);
        self->focused_workspace = g_strdup(event->workspace.name);
    }

    sway_client_ipc_get_workspaces_req(self->socket_fd);
};

static void handle_ipc_event_outputs(WMServiceSway *self,
                                     sway_client_ipc_msg *msg) {
    g_debug(
        "window_manager_service_sway.c:handle_ipc_event_outputs() "
        "received output event, getting latest output listing.");
    sway_client_ipc_get_outputs_req(self->socket_fd);
};

static void on_ipc_recv_dispatch(WMServiceSway *self,
                                 sway_client_ipc_msg *msg) {
    g_debug(
        "window_manager_service_sway.c:on_ipc_recv_dispatch() "
        "dispatching ipc message.");
    switch (msg->type) {
        case IPC_GET_WORKSPACES: {
            handle_ipc_get_workspaces(self, msg);
            break;
        }
        case IPC_GET_OUTPUTS: {
            handle_ipc_get_outputs(self, msg);
            break;
        }
        case IPC_SUBSCRIBE: {
            self->subscribed = sway_client_ipc_subscribe_resp(msg);
            if (!self->subscribed) {
                g_error(
                    "window_manager_service_sway.c:on_ipc_recv_dispatch() "
                    "failed to subscribe to events.");
            }
            g_info(
                "window_manager_service_sway.c:on_ipc_recv_dispatch() "
                "sway_client_ipc_subscribe_resp received subscribed: %s",
                self->subscribed ? "true" : "false");
            break;
        }
        case IPC_EVENT_WORKSPACE: {
            if (!self->workspaces) {
                g_debug(
                    "window_manager_service_sway.c:on_ipc_recv_dispatch() "
                    "ignoring event until initial sync.");
                break;
            }
            handle_ipc_event_workspaces(self, msg);
            break;
        }
        case IPC_EVENT_OUTPUT: {
            if (!self->outputs) {
                g_debug(
                    "window_manager_service_sway.c:on_ipc_recv_dispatch() "
                    "ignoring event until initial sync.");
                break;
            }
            handle_ipc_event_outputs(self, msg);
            break;
        }
    }
}

static gboolean on_ipc_recv(gint fd, GIOCondition condition,
                            WMServiceSway *self) {
    sway_client_ipc_msg msg;

    g_debug(
        "window_manager_service_sway.c:on_ipc_recv() "
        "received ipc message.");

    // TODO: implement recovery from this.
    if (condition & G_IO_HUP || condition & G_IO_ERR) {
        g_debug(
            "window_manager_service_sway.c:handle_ipc_recv() "
            "received G_IO_HUP or G_IO_ERR, GLib polling stopped.");
        return false;
    }

    // retrieve ipc msg
    if (sway_client_ipc_recv(self->socket_fd, &msg) != 0) {
        g_debug(
            "window_manager_service_sway.c:handle_ipc_recv() "
            "failed to receive ipc message.");
        return false;
    }

    on_ipc_recv_dispatch(self, &msg);

    return true;
}

static void on_sort_alphabetical_changed(GSettings *settings, gchar *key,
                                         WMServiceSway *self) {
    g_debug(
        "window_manager_service_sway.c:on_sort_alphabetical_changed() "
        "sort-alphabetical setting changed, updating workspaces.");

    // perform workspaces request
    sway_client_ipc_get_workspaces_req(self->socket_fd);
}

static void wm_service_sway_init(WMServiceSway *self) {
    self->socket_path = sway_client_find_socket_path();
    if (!self->socket_path)
        g_error(
            "window_manager_service_sway.c:wm_service_sway_init "
            "failed to find socket path.");

    g_debug(
        "window_manager_service_sway.c:wm_service_sway_init "
        "found socket path: %s",
        self->socket_path);

    self->socket_fd = sway_client_ipc_connect(self->socket_path);
    if (self->socket_fd < 0)
        g_error(
            "window_manager_service_sway.c:wm_service_sway_init "
            "failed to connect to socket.");
    g_debug(
        "window_manager_service_sway.c:wm_service_sway_init "
        "connected to self ipc socket: %d.",
        self->socket_fd);

    // add our connected socket to GLib event loop.
    self->poll_id =
        g_unix_fd_add(self->socket_fd, G_IO_IN | G_IO_HUP | G_IO_ERR,
                      (GUnixFDSourceFunc)on_ipc_recv, self);

    // connect to 'org.ldelossa.way-shell.window-manager.workspaces' setting
    self->settings = g_settings_new("org.ldelossa.way-shell.window-manager");

    // wire into sort-alphabetical value
    g_signal_connect(self->settings, "changed::sort-workspaces-alphabetical",
                     G_CALLBACK(on_sort_alphabetical_changed), self);
};

// Initializes the sway wm service.
int wm_service_sway_global_init(void) {
    g_debug(
        "window_manager_service_sway.c:wm_service_sway_global_init() "
        "initializing global service.");

    global = g_object_new(WM_SERVICE_SWAY_TYPE, NULL);

    // subscribe to desired events
    sway_client_ipc_subscribe_req(
        global->socket_fd, (int[]){IPC_EVENT_WORKSPACE, IPC_EVENT_OUTPUT}, 2);

    // get initial listing of workspaces
    sway_client_ipc_get_workspaces_req(global->socket_fd);

    // get initial listing of outputs
    sway_client_ipc_get_outputs_req(global->socket_fd);

    return 0;
};

GPtrArray *wm_service_sway_get_workspaces(WMServiceSway *self) {
    if (!self->workspaces) {
        g_warning(
            "window_manager_service_sway.c:wm_service_sway_get_workspaces() "
            "workspaces not initialized.");
        return NULL;
    }

    return g_ptr_array_ref(self->workspaces);
}

GPtrArray *wm_service_sway_get_outputs(WMServiceSway *self) {
    if (!self->outputs) {
        g_warning(
            "window_manager_service_sway.c:wm_service_sway_get_outputs() "
            "outputs not initialized.");
        return NULL;
    }

    return g_ptr_array_ref(self->outputs);
}

int wm_service_sway_focus_workspace(WMServiceSway *self, WMWorkspace *ws) {
    if (!self->workspaces) {
        g_warning(
            "window_manager_service_sway.c:wm_service_sway_focus_workspace() "
            "workspaces not initialized.");
        return -1;
    }
    return sway_client_ipc_focus_workspace(self->socket_fd, ws);
}

int wm_service_sway_current_ws_to_output(WMServiceSway *self, WMOutput *o) {
    if (!o) {
        g_warning(
            "window_manager_service_sway.c:wm_service_sway_current_ws_to_"
            "output() "
            "outputs not initialized.");
        return -1;
    }
    return sway_client_ipc_move_ws_to_output(self->socket_fd, o->name);
}

// Get the global wm service
// Will return NULL if `wm_service_sway_global_init` has not been called.
WMServiceSway *wm_service_sway_get_global() { return global; };
