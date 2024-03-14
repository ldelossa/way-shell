#include "wayland_service.h"

#include <adwaita.h>
#include <gdk/wayland/gdkwayland.h>
#include <glib-2.0/glib-unix.h>
#include <sys/cdefs.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

static WaylandService *global = NULL;

enum signals { signals_n };

struct _WaylandService {
    GObject parent_instance;
    struct wl_display *display;
    struct wl_registry *registry;
    int fd;
    GHashTable *index;
    WaylandOutputDB *outputs;
};

static guint service_signals[signals_n] = {0};

G_DEFINE_TYPE(WaylandService, wayland_service, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init methods for this GObject
static void wayland_service_dispose(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(wayland_service_parent_class)->dispose(gobject);
};

static void wayland_service_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(wayland_service_parent_class)->finalize(gobject);
};

static void wayland_service_class_init(WaylandServiceClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = wayland_service_dispose;
    object_class->finalize = wayland_service_finalize;
};

gboolean on_fd_read(gint fd, GIOCondition condition, gpointer user_data) {
    g_debug("wayland_service.c:on_fd_read(): fd: %d, condition: %d", fd,
            condition);

    WaylandService *self = (WaylandService *)user_data;
    if (condition & (G_IO_ERR | G_IO_HUP)) {
        g_error("Wayland socket error");
    }

    // wl_display_dispatch will both read from the socket and then dispatch
    // messages from the default queue to listeners
    if (wl_display_dispatch(self->display) == -1) {
        g_error("Wayland dispatch failed");
    }
    return TRUE;
}

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface,
                                   uint32_t version) {
    g_debug(
        "wayland_service.c:registry_handle_global(): name: %d, interface: %s, "
        "version: %d",
        name, interface, version);
}

static void registry_handle_global_remove(void *data,
                                          struct wl_registry *registry,
                                          uint32_t name) {
    g_debug("wayland_service.c:registry_handle_global_remove(): name: %d",
            name);
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

static void wayland_service_init(WaylandService *self) {
    self->display = wl_display_connect(NULL);
    if (!self->display) {
        g_error("Failed to connect to Wayland display");
    }

    self->fd = wl_display_get_fd(self->display);
    self->registry = wl_display_get_registry(self->display);

    // create index hash table which hashes uint32_t to pointers
    self->index =
        g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);

    // create WaylandOutput database.
    self->outputs = g_new0(WaylandOutputDB, 1);
    self->outputs->header.type = WL_OUTPUT_DB;
    self->outputs->db =
        g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);

    // add registry listener
    wl_registry_add_listener(self->registry, &registry_listener, self);

    // add wayland fd to event loop
    g_unix_fd_add(self->fd, G_IO_IN | G_IO_ERR | G_IO_HUP, on_fd_read, self);

    // issue roundtrip
    wl_display_roundtrip(self->display);

    g_debug(
        "wayland_service.c:wayland_service_init Wayland service initialized");

    return;
};

// no error checking on below function since we bail out if wayland socket is
// not available.

int wayland_service_global_init(void) {
    global = g_object_new(WAYLAND_SERVICE_TYPE, NULL);
    return 0;
}

WaylandService *wayland_service_get_global() { return global; }
