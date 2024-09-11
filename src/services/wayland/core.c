#include "core.h"

#include <adwaita.h>
#include <fcntl.h>
#include <gdk/wayland/gdkwayland.h>
#include <glib-2.0/glib-unix.h>
#include <sys/cdefs.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

#include "foreign_toplevel_service/foreign_toplevel.h"
#include "gamma_control_service/gamma.h"
#include "keyboard_shortcuts_inhibit_service/ksi.h"
#include "wl_output.h"
#include "wl_seat.h"
#include "wlr-foreign-toplevel-management-unstable-v1.h"
#include "wlr-gamma-control-unstable-v1.h"

static WaylandCoreService *global = NULL;

struct _WaylandCoreService {
    GObject parent_instance;

    // file descriptor for wayland socket
    int fd;

    // wayland display
    struct wl_display *display;

    // wayland registry
    struct wl_registry *registry;

    // wlr gamma control manager for adjusting gamma
    struct zwlr_gamma_control_manager_v1 *gamma_control_manager;

    // inventorys globals by their uint 'name' value provided when listening
    // for changes to global objects on the registry.
    GHashTable *globals;

    // WaylandSeat structs mapped by wl_seat pointers
    GHashTable *seats;

    // WaylandOutput structs mapped to wl_output pointers
    GHashTable *outputs;
};

guint wayland_core_service_signals[wl_signals_n] = {0};

G_DEFINE_TYPE(WaylandCoreService, wayland_core_service, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init methods for this GObject
static void wayland_core_service_dispose(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(wayland_core_service_parent_class)->dispose(gobject);
};

static void wayland_core_service_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(wayland_core_service_parent_class)->finalize(gobject);
};

static void wayland_core_service_class_init(WaylandCoreServiceClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = wayland_core_service_dispose;
    object_class->finalize = wayland_core_service_finalize;

    wayland_core_service_signals[wl_output_added] = g_signal_new(
        "wl-output-added", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_HASH_TABLE, G_TYPE_POINTER);

    wayland_core_service_signals[wl_output_removed] = g_signal_new(
        "wl-output-removed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);
};

gboolean on_fd_read(gint fd, GIOCondition condition, gpointer user_data) {
    g_debug("core.c:on_fd_read(): fd: %d, condition: %d", fd, condition);

    WaylandCoreService *self = (WaylandCoreService *)user_data;
    if (condition & (G_IO_ERR | G_IO_HUP)) {
        g_error("Wayland socket error");
    }

    // wl_display_dispatch will both read from the socket and then dispatch
    // messages from the default queue to listeners
    if (wl_display_dispatch(self->display) == -1) {
        g_error("Wayland dispatch failed");
    }

    // flush display, handle EAGAIN in a loop.
    int ret = 0;
    ret = wl_display_flush(self->display);
    while (ret == -1 && errno == EAGAIN) ret = wl_display_flush(self->display);
    if (ret == -1) g_error("Wayland flush failed");

    return TRUE;
}

// main global registry handler function.
// register globals and construct services based on global registry inventory.
static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface,
                                   uint32_t version) {
    g_debug(
        "core.c:registry_handle_global(): name: %d, interface: %s, "
        "version: %d",
        name, interface, version);

    WaylandCoreService *self = (WaylandCoreService *)data;

    // register wl_seat_listener
    if (strcmp(interface, "wl_seat") == 0) {
        struct wl_seat *wl_seat =
            wl_registry_bind(registry, name, &wl_seat_interface, version);
        wl_seat_register(self, wl_seat, name);
    }

    // register wl_output listener
    if (strcmp(interface, "wl_output") == 0) {
        struct wl_output *wl_output =
            wl_registry_bind(registry, name, &wl_output_interface, version);
        wl_output_register(self, wl_output, name);
    }

    if (strcmp(interface, "zwp_keyboard_shortcuts_inhibit_manager_v1") == 0) {
        // we don't need to bind a proxy, we can use GDK libraries for this.
        wayland_ksi_service_global_init(self);
    }

    // register zwlr_foreign_toplevel_manager_v1_listener
    if (strcmp(interface, "zwlr_foreign_toplevel_manager_v1") == 0) {
        struct zwlr_foreign_toplevel_manager_v1 *mgr = wl_registry_bind(
            registry, name, &zwlr_foreign_toplevel_manager_v1_interface,
            version);
        wayland_foreign_toplevel_service_global_init(self, mgr);
    }

    // register zwlr_gamma_control_manager_v1
    if (strcmp(interface, "zwlr_gamma_control_manager_v1") == 0) {
        struct zwlr_gamma_control_manager_v1 *mgr = wl_registry_bind(
            registry, name, &zwlr_gamma_control_manager_v1_interface, version);
        wayland_gamma_control_service_global_init(self, mgr);
    }
}

static void registry_handle_global_remove(void *data,
                                          struct wl_registry *registry,
                                          uint32_t name) {
    g_debug("core.c:registry_handle_global_remove(): name: %d", name);

    WaylandCoreService *self = (WaylandCoreService *)data;

    WaylandHeader *header =
        g_hash_table_lookup(self->globals, GUINT_TO_POINTER(name));

    if (!header) return;

    switch (header->type) {
        case WL_REGISTRY:
            break;
        case WL_SEAT:
            wl_seat_remove(self, (WaylandSeat *)header);
            break;
        case WL_OUTPUT:
            wl_output_remove(self, (WaylandOutput *)header);
            break;
        default:
            return;
    }

    // flush display to sync changes.
    wl_display_flush(self->display);
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

static void wayland_core_service_init(WaylandCoreService *self) {
    self->display = wl_display_connect(NULL);
    if (!self->display) {
        g_error("Failed to connect to Wayland display");
    }

    self->fd = wl_display_get_fd(self->display);
    self->registry = wl_display_get_registry(self->display);

    // setup hash tables
    self->globals = g_hash_table_new(g_direct_hash, g_direct_equal);
    self->seats = g_hash_table_new(g_direct_hash, g_direct_equal);
    self->outputs = g_hash_table_new(g_direct_hash, g_direct_equal);

    // add registry listener
    wl_registry_add_listener(self->registry, &registry_listener, self);

    // add wayland fd to event loop
    g_unix_fd_add(self->fd, G_IO_IN | G_IO_ERR | G_IO_HUP, on_fd_read, self);

    // issue a round-trip so we block until all globals are all bound and
    // listeners are registered.
    wl_display_roundtrip(self->display);
    // all listeners are created and they may have queued some requests, so now
    // flush our display once again.
    wl_display_flush(self->display);

    g_debug("core.c:wayland_core_service_init Wayland service initialized");

    return;
};

GHashTable *wayland_core_service_get_globals(WaylandCoreService *self) {
    return self->globals;
}

GHashTable *wayland_core_service_get_seats(WaylandCoreService *self) {
    return self->seats;
}

GHashTable *wayland_core_service_get_outputs(WaylandCoreService *self) {
    return self->outputs;
}

struct wl_display *wayland_core_service_get_display(WaylandCoreService *self) {
    return self->display;
}

// no error checking on below function since we bail out if wayland socket is
// not available.
int wayland_core_service_global_init(void) {
    global = g_object_new(WAYLAND_CORE_SERVICE_TYPE, NULL);
    return 0;
}

WaylandCoreService *wayland_core_service_get_global() { return global; }
