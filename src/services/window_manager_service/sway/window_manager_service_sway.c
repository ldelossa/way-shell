#include "window_manager_service_sway.h"

#include <adwaita.h>
#include <glib-2.0/glib-unix.h>

#include "../window_manager_service.h"
#include "ipc.h"
#include "sway_client.h"

static WMServiceSway *global = NULL;

enum signals { workspaces_changed, signals_n };

struct _WMServiceSway {
    GObject parent_instance;
    GPtrArray *workspaces;
    char *socket_path;
    int socket_fd;
    guint poll_id;
    gboolean polling;
    gboolean subscribed;
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
        g_ptr_array_sort(self->workspaces, (GCompareFunc)compare_workspace_name);

    // emit signal
    g_signal_emit(self, service_signals[workspaces_changed], 0,
                  self->workspaces);
}

static void handle_ipc_event_workspaces(WMServiceSway *self,
                                        sway_client_ipc_msg *msg) {
    // perform get workspaces req to get latest workspace's state.
    // this maybe slower (another round tip) but works way smoother
    // on spam events like monitor create and deletes.
    g_debug(
        "window_manager_service_sway.c:handle_ipc_event_workspaces() "
        "received workspace event, getting latest workspace listing.");
    sway_client_ipc_get_workspaces_req(self->socket_fd);
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
        case IPC_SUBSCRIBE: {
            self->subscribed = sway_client_ipc_subscribe_resp(msg);
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
            "received G_IO_HUP or G_IO_ERR, GLib polling stoped.");
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
    sway_client_ipc_subscribe_req(global->socket_fd,
                                  (int[]){IPC_EVENT_WORKSPACE}, 1);

    // get initial listing of workspaces
    sway_client_ipc_get_workspaces_req(global->socket_fd);

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

int wm_service_sway_focus_workspace(WMServiceSway *self, gchar *name) {
    if (!self->workspaces) {
        g_warning(
            "window_manager_service_sway.c:wm_service_sway_focus_workspace() "
            "workspaces not initialized.");
        return NULL;
    }
    return sway_client_ipc_focus_workspace(self->socket_fd, name);
}

// Get the global wm service
// Will return NULL if `wm_service_sway_global_init` has not been called.
WMServiceSway *wm_service_sway_get_global() { return global; };
