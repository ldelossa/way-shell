#include "wayland_service.h"

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

#include "./wlr-foreign-toplevel-management-unstable-v1.h"
#include "./wlr-gamma-control-unstable-v1.h"
#include "colorramp.h"

static WaylandService *global = NULL;

enum signals {
    top_level_changed,
    top_level_removed,
    output_added,
    output_removed,
    gamma_control_enabled,
    gamma_control_disabled,
    signals_n
};

struct _WaylandService {
    GObject parent_instance;

    // file descriptor for wayland socket
    int fd;

    // wayland display
    struct wl_display *display;

    // wayland registry
    struct wl_registry *registry;

    // wlr gamma control manager for adjusting gamma
    struct zwlr_gamma_control_manager_v1 *gamma_control_manager;

    // keyboard short inhibitor
    gboolean shortcuts_inhitibed;
    GdkToplevel *shorcuts_inhibited_toplevel;

    // inventorys globals by their uint 'name' value provided when listening
    // for changes to global objects on the registry.
    GHashTable *globals;

    // WaylandSeat structs mapped by wl_seat pointers
    GHashTable *seats;

    // WaylandWLRForeignTopLevel structs mapped by
    // zwlr_foreign_top_level_handle_v1 structs
    GHashTable *toplevels;

    // WaylandOutput structs mapped to wl_output pointers
    GHashTable *outputs;

    // Whether gamma is being controlled
    gboolean gamma_control_enabled;
    // List of created gamma controllers
    GHashTable *gamma_controllers;
    // The last seen temperature
    double temperature;

    // Monitors org.ldelossa.way-shell.window-manager.ignored-toplevels-app-ids
    // and org.ldelossa.way-shell.window-manager.ignored-toplevels-titles
    GSettings *settings;
    GHashTable *ignored_toplevel_app_ids;
    GHashTable *ignored_toplevel_titles;
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

    service_signals[top_level_changed] = g_signal_new(
        "top-level-changed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_HASH_TABLE, G_TYPE_POINTER);

    service_signals[top_level_removed] = g_signal_new(
        "top-level-removed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);

    service_signals[output_added] = g_signal_new(
        "output-added", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0, NULL,
        NULL, NULL, G_TYPE_NONE, 2, G_TYPE_HASH_TABLE, G_TYPE_POINTER);

    service_signals[output_removed] = g_signal_new(
        "output-removed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0, NULL,
        NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);

    service_signals[gamma_control_enabled] =
        g_signal_new("gamma-control-enabled", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    service_signals[gamma_control_disabled] =
        g_signal_new("gamma-control-disabled", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
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

    // flush display, handle EAGAIN in a loop.
    int ret = 0;
    ret = wl_display_flush(self->display);
    while (ret == -1 && errno == EAGAIN) ret = wl_display_flush(self->display);
    if (ret == -1) g_error("Wayland flush failed");

    return TRUE;
}

static void toplevel_handle_title(
    void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
    const char *title) {
    g_debug("wayland_service.c:toplevel_handle_title(): title: %s", title);

    WaylandService *self = (WaylandService *)data;
    WaylandWLRForeignTopLevel *top_level =
        g_hash_table_lookup(self->toplevels, handle);

    if (!top_level) {
        return;
    }

    top_level->title = g_strdup(title);

    g_debug("wayland_service.c:toplevel_handle_title(): top_level->title: %s",
            top_level->title);
}

static void seat_listener_capabilities(void *data, struct wl_seat *seat,
                                       uint32_t capabilities) {
    g_debug("wayland_service.c:seat_listener_capabilities(): capabilities: %d",
            capabilities);
    // no-op, we don't care about capabilities
}

static void seat_listener_name(void *data, struct wl_seat *seat,
                               const char *name) {
    g_debug("wayland_service.c:seat_listener_name(): name: %s", name);

    WaylandService *self = (WaylandService *)data;
    WaylandSeat *seat_data = g_hash_table_lookup(self->seats, seat);
    seat_data->name = g_strdup(name);

    g_debug("wayland_service.c:seat_listener_name(): seat_data->name: %s",
            seat_data->name);
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_listener_capabilities,
    .name = seat_listener_name,
};

static void toplevel_handle_app_id(
    void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
    const char *app_id) {
    g_debug("wayland_service.c:toplevel_handle_app_id(): app_id: %s", app_id);

    WaylandService *self = (WaylandService *)data;
    WaylandWLRForeignTopLevel *top_level =
        g_hash_table_lookup(self->toplevels, handle);

    if (!top_level) {
        return;
    }

    top_level->app_id = g_strdup(app_id);

    g_debug("wayland_service.c:toplevel_handle_app_id(): top_level->app_id: %s",
            top_level->app_id);
}

static void toplevel_handle_closed(
    void *data, struct zwlr_foreign_toplevel_handle_v1 *handle) {
    g_debug(
        "wayland_service.c:toplevel_handle_closed(): toplevel handle closed");

    WaylandService *self = (WaylandService *)data;
    WaylandWLRForeignTopLevel *toplevel =
        g_hash_table_lookup(self->toplevels, handle);

    if (!toplevel) {
        return;
    }

    g_debug(
        "wayland_service.c:toplevel_handle_closed(): toplevel->app_id: %s, "
        "toplevel->title: %s",
        toplevel->app_id, toplevel->title);

    // a toplevel without a valid app_id and title was never advertised to the
    // rest of Way-Shell, so no reason to signal its removal.
    if (!toplevel->app_id || !toplevel->title) goto remove;

    gboolean ignored = false;
    ignored =
        g_hash_table_contains(self->ignored_toplevel_app_ids, toplevel->app_id);

    if (!ignored)
        ignored = g_hash_table_contains(self->ignored_toplevel_titles,
                                        toplevel->title);

    if (!ignored)
        g_signal_emit(self, service_signals[top_level_removed], 0, toplevel);

remove:
    // remove it from our inventory
    g_hash_table_remove(self->toplevels, handle);

    // destroy the proxy at the Wayland server.
    zwlr_foreign_toplevel_handle_v1_destroy(handle);

    // free any memory we alloc'd
    g_free(toplevel->app_id);
    g_free(toplevel->title);
    g_free(toplevel);
}

static void toplevel_handle_done(
    void *data, struct zwlr_foreign_toplevel_handle_v1 *handle) {
    g_debug("wayland_service.c:toplevel_handle_done(): toplevel handle done");

    WaylandService *self = (WaylandService *)data;
    WaylandWLRForeignTopLevel *toplevel =
        g_hash_table_lookup(self->toplevels, handle);

    if (!toplevel) {
        return;
    }

    // debug toplevel details
    g_debug(
        "wayland_service.c:toplevel_handle_done(): toplevel->app_id: %s, "
        "toplevel->title: %s, toplevel->entered: %d, toplevel->activated: %d",
        toplevel->app_id, toplevel->title, toplevel->entered,
        toplevel->activated);

    // if we don't have a valid app_id and title, don't bother signaling this
    // toplevel, the rest of Way-Shell expects these fields.
    if (!toplevel->app_id || !toplevel->title) goto reset;

    gboolean ignored = false;
    ignored =
        g_hash_table_contains(self->ignored_toplevel_app_ids, toplevel->app_id);

    if (!ignored)
        ignored = g_hash_table_contains(self->ignored_toplevel_titles,
                                        toplevel->title);

    if (!ignored)
        g_signal_emit(self, service_signals[top_level_changed], 0,
                      self->toplevels, toplevel);

reset:
    // reset bools after handlers read event
    toplevel->entered = FALSE;
    toplevel->activated = FALSE;
}

static void toplevel_handle_output_enter(
    void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
    struct wl_output *output) {
    g_debug(
        "wayland_service.c:toplevel_handle_output_enter(): toplevel handle "
        "output enter");

    WaylandService *self = (WaylandService *)data;
    WaylandWLRForeignTopLevel *top_level =
        g_hash_table_lookup(self->toplevels, handle);

    if (!top_level) {
        return;
    }

    top_level->entered = TRUE;
}

static void toplevel_handle_output_leave(
    void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
    struct wl_output *output) {
    g_debug(
        "wayland_service.c:toplevel_handle_output_leave(): toplevel handle "
        "output leave");

    WaylandService *self = (WaylandService *)data;
    WaylandWLRForeignTopLevel *top_level =
        g_hash_table_lookup(self->toplevels, handle);

    if (!top_level) {
        return;
    }

    top_level->entered = FALSE;
}

static void toplevel_handle_parent(
    void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
    struct zwlr_foreign_toplevel_handle_v1 *parent) {
    g_debug(
        "wayland_service.c:toplevel_handle_parent(): toplevel handle parent");
}

static void toplevel_handle_state(
    void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
    struct wl_array *state) {
    g_debug("wayland_service.c:toplevel_handle_state(): toplevel handle state");

    WaylandService *self = (WaylandService *)data;
    WaylandWLRForeignTopLevel *top_level =
        g_hash_table_lookup(self->toplevels, handle);

    if (!top_level) {
        return;
    }

    // check if activated
    top_level->activated = FALSE;
    for (size_t i = 0; i < state->size; i++) {
        uint32_t *state_ptr = (uint32_t *)state->data + i;
        if (*state_ptr == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED) {
            top_level->activated = TRUE;
        }
    }
}

static const struct zwlr_foreign_toplevel_handle_v1_listener
    toplevel_handle_listener = {
        .title = toplevel_handle_title,
        .app_id = toplevel_handle_app_id,
        .output_enter = toplevel_handle_output_enter,
        .output_leave = toplevel_handle_output_leave,
        .state = toplevel_handle_state,
        .done = toplevel_handle_done,
        .closed = toplevel_handle_closed,
        .parent = toplevel_handle_parent,
};

static void toplevel_mgr_new_toplevel(
    void *data, struct zwlr_foreign_toplevel_manager_v1 *mgr,
    struct zwlr_foreign_toplevel_handle_v1 *wayland_toplevel) {
    g_debug(
        "wayland_service.c:toplevel_mgr_new_toplevel(): new toplevel handle");

    WaylandService *self = (WaylandService *)data;

    WaylandWLRForeignTopLevel *toplevel = g_new0(WaylandWLRForeignTopLevel, 1);
    toplevel->header.type = WL_REGISTRY;
    toplevel->toplevel = wayland_toplevel;

    g_hash_table_insert(self->toplevels, wayland_toplevel, toplevel);

    zwlr_foreign_toplevel_handle_v1_add_listener(
        wayland_toplevel, &toplevel_handle_listener, self);
}

static void toplevel_mgr_finished(
    void *data, struct zwlr_foreign_toplevel_manager_v1 *mgr) {
    g_debug(
        "wayland_service.c:toplevel_mgr_finished(): toplevel manager "
        "finished");
}

static const struct zwlr_foreign_toplevel_manager_v1_listener
    toplevel_mgr_listener = {
        .toplevel = toplevel_mgr_new_toplevel,
        .finished = toplevel_mgr_finished,
};

static void wl_output_handle_description(void *data, struct wl_output *output,
                                         const char *description) {
    g_debug("wayland_service.c:wl_output_handle_description(): description: %s",
            description);

    WaylandService *self = (WaylandService *)data;
    WaylandOutput *output_data = g_hash_table_lookup(self->outputs, output);
    output_data->desc = g_strdup(description);

    g_debug(
        "wayland_service.c:wl_output_handle_description(): output_data->desc: "
        "%s",
        output_data->desc);
}

void wayland_wlr_bluelight_filter(WaylandService *self, double temperature);

static void wl_output_handle_done(void *data, struct wl_output *output) {
    g_debug("wayland_service.c:wl_output_handle_done(): output done");

    WaylandService *self = (WaylandService *)data;
    WaylandOutput *output_data = g_hash_table_lookup(self->outputs, output);

    if (!output_data) {
        return;
    }

    output_data->initialized = TRUE;

    // if gamma controls are enabled, we should apply it to the new monitor
    if (self->gamma_control_enabled) {
        wayland_wlr_bluelight_filter(self, self->temperature);
    }

    g_signal_emit(self, service_signals[output_added], 0, self->outputs,
                  output);

    g_debug(
        "wayland_service.c:wl_output_handle_done(): output_data->name: %s, "
        "output_data->desc: %s, output_data->make: %s, output_data->model: %s",
        output_data->name, output_data->desc, output_data->make,
        output_data->model);
}

static void wl_output_handle_geometry(void *data, struct wl_output *output,
                                      int32_t x, int32_t y,
                                      int32_t physical_width,
                                      int32_t physical_height, int32_t subpixel,
                                      const char *make, const char *model,
                                      int32_t transform) {
    g_debug(
        "wayland_service.c:wl_output_handle_geometry(): x: %d, y: %d, "
        "physical_width: %d, physical_height: %d, subpixel: %d, make: %s, "
        "model: %s, transform: %d",
        x, y, physical_width, physical_height, subpixel, make, model,
        transform);

    WaylandService *self = (WaylandService *)data;
    WaylandOutput *output_data = g_hash_table_lookup(self->outputs, output);
    output_data->make = g_strdup(make);
    output_data->model = g_strdup(model);

    g_debug(
        "wayland_service.c:wl_output_handle_geometry(): output_data->make: %s, "
        "output_data->model: %s",
        output_data->make, output_data->model);
}

static void wl_output_handle_mode(void *data, struct wl_output *output,
                                  uint32_t flags, int32_t width, int32_t height,
                                  int32_t refresh) {
    g_debug(
        "wayland_service.c:wl_output_handle_mode(): flags: %d, width: %d, "
        "height: %d, refresh: %d",
        flags, width, height, refresh);
}

static void wl_output_handle_name(void *data, struct wl_output *output,
                                  const char *name) {
    g_debug("wayland_service.c:wl_output_handle_name(): name: %s", name);

    WaylandService *self = (WaylandService *)data;
    WaylandOutput *output_data = g_hash_table_lookup(self->outputs, output);
    output_data->name = g_strdup(name);

    g_debug("wayland_service.c:wl_output_handle_name(): output_data->name: %s",
            output_data->name);
}

static void wl_output_handle_scale(void *data, struct wl_output *output,
                                   int32_t scale) {
    g_debug("wayland_service.c:wl_output_handle_scale(): scale: %d", scale);
}

static const struct wl_output_listener wl_output_listener = {
    .description = wl_output_handle_description,
    .done = wl_output_handle_done,
    .geometry = wl_output_handle_geometry,
    .mode = wl_output_handle_mode,
    .name = wl_output_handle_name,
    .scale = wl_output_handle_scale,
};

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface,
                                   uint32_t version) {
    g_debug(
        "wayland_service.c:registry_handle_global(): name: %d, interface: %s, "
        "version: %d",
        name, interface, version);

    WaylandService *self = (WaylandService *)data;

    // register wl_seat_listener
    if (strcmp(interface, "wl_seat") == 0) {
        struct wl_seat *wayland_seat =
            wl_registry_bind(registry, name, &wl_seat_interface, version);
        WaylandSeat *seat = g_new0(WaylandSeat, 1);
        seat->header.type = WL_SEAT;
        seat->seat = wayland_seat;
        seat->name = NULL;
        g_hash_table_insert(self->globals, GUINT_TO_POINTER(name), seat);
        g_hash_table_insert(self->seats, wayland_seat, seat);
        wl_seat_add_listener(wayland_seat, &seat_listener, self);
    }

    // register zwlr_foreign_toplevel_manager_v1_listener
    if (strcmp(interface, "zwlr_foreign_toplevel_manager_v1") == 0) {
        struct zwlr_foreign_toplevel_manager_v1 *toplevel_mgr =
            wl_registry_bind(registry, name,
                             &zwlr_foreign_toplevel_manager_v1_interface,
                             version);
        zwlr_foreign_toplevel_manager_v1_add_listener(
            toplevel_mgr, &toplevel_mgr_listener, self);
    }

    // register wl_output listener
    if (strcmp(interface, "wl_output") == 0) {
        struct wl_output *wayland_output =
            wl_registry_bind(registry, name, &wl_output_interface, version);
        WaylandOutput *output = g_new0(WaylandOutput, 1);
        output->header.type = WL_OUTPUT;
        output->output = wayland_output;
        output->name = NULL;
        output->desc = NULL;
        output->make = NULL;
        output->model = NULL;
        output->initialized = FALSE;
        g_hash_table_insert(self->outputs, wayland_output, output);
        g_hash_table_insert(self->globals, GUINT_TO_POINTER(name), output);
        wl_output_add_listener(wayland_output, &wl_output_listener, self);
    }

    // register zwlr_gamma_control_manager_v1
    if (strcmp(interface, "zwlr_gamma_control_manager_v1") == 0) {
        self->gamma_control_manager = wl_registry_bind(
            registry, name, &zwlr_gamma_control_manager_v1_interface, version);
    }
}

static void wayland_seat_remove(WaylandService *self, WaylandSeat *seat) {
    g_debug("wayland_service.c:wayland_seat_remove(): seat: %s", seat->name);

    // remove from seats hash table
    g_hash_table_remove(self->seats, seat->seat);

    // release seat from wayland
    wl_seat_release(seat->seat);

    // free seat
    g_free(seat->name);
    g_free(seat);
};

static void wayland_output_remove(WaylandService *self, WaylandOutput *output) {
    g_debug("wayland_service.c:wayland_output_remove(): output: %s",
            output->name);

    // emit remove signal with output before we free it
    g_signal_emit(self, service_signals[output_removed], 0, output);

    // remove from outputs hash table
    g_hash_table_remove(self->outputs, output->output);

    // release output from wayland
    wl_output_release(output->output);

    // free output
    g_free(output->name);
    g_free(output->desc);
    g_free(output->make);
    g_free(output->model);
    g_free(output);
}

static void registry_handle_global_remove(void *data,
                                          struct wl_registry *registry,
                                          uint32_t name) {
    g_debug("wayland_service.c:registry_handle_global_remove(): name: %d",
            name);

    WaylandService *self = (WaylandService *)data;

    WaylandHeader *header =
        g_hash_table_lookup(self->globals, GUINT_TO_POINTER(name));
    if (!header) return;

    switch (header->type) {
        case WL_REGISTRY:
            break;
        case WL_SEAT:
            wayland_seat_remove(self, (WaylandSeat *)header);
            break;
        case WL_OUTPUT:
            wayland_output_remove(self, (WaylandOutput *)header);
            break;
    }

    // remove from globals after interface specific cleanup above.
    g_hash_table_remove(self->globals, GUINT_TO_POINTER(name));

    // flush display to sync changes.
    wl_display_flush(self->display);
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

static void on_ignored_toplevels_app_ids_changed(GSettings *settings,
                                                 gchar *key,
                                                 WaylandService *self) {
    g_debug("wayland_service.c:on_ignored_toplevels_app_ids_changed(): key: %s",
            key);

    // clear the hash table
    g_hash_table_remove_all(self->ignored_toplevel_app_ids);

    gchar *encoded_list =
        g_settings_get_string(self->settings, "ignored-toplevels-app-ids");

    // if encoded_list is empty just return
    if (g_strcmp0(encoded_list, "") == 0) return;

    // split list by colons
    gchar **app_ids = g_strsplit(encoded_list, ":", -1);

    // dup and hash each string
    for (gchar **app_id = app_ids; *app_id; app_id++) {
        g_hash_table_insert(self->ignored_toplevel_app_ids, *app_id, *app_id);
    }
}

static void on_ignored_toplevels_titles_changed(GSettings *settings, gchar *key,
                                                WaylandService *self) {
    g_debug("wayland_service.c:on_ignored_toplevels_titles_changed(): key: %s",
            key);

    // clear the hash table
    g_hash_table_remove_all(self->ignored_toplevel_titles);

    gchar *encoded_list =
        g_settings_get_string(self->settings, "ignored-toplevels-titles");

    // if encoded_list is empty just return
    if (g_strcmp0(encoded_list, "") == 0) return;

    // split list by colons
    gchar **titles = g_strsplit(encoded_list, ":", -1);

    // dup and hash each string
    for (gchar **title = titles; *title; title++) {
        g_hash_table_insert(self->ignored_toplevel_titles, *title, *title);
    }
}

static void wayland_service_init(WaylandService *self) {
    self->display = wl_display_connect(NULL);
    if (!self->display) {
        g_error("Failed to connect to Wayland display");
    }

    self->fd = wl_display_get_fd(self->display);
    self->registry = wl_display_get_registry(self->display);

    // setup hash tables
    self->globals = g_hash_table_new(g_direct_hash, g_direct_equal);
    self->seats = g_hash_table_new(g_direct_hash, g_direct_equal);
    self->toplevels = g_hash_table_new(g_direct_hash, g_direct_equal);
    self->outputs = g_hash_table_new(g_direct_hash, g_direct_equal);
    self->gamma_controllers = g_hash_table_new(g_direct_hash, g_direct_equal);
    self->ignored_toplevel_app_ids = g_hash_table_new(g_str_hash, g_str_equal);
    self->ignored_toplevel_titles = g_hash_table_new(g_str_hash, g_str_equal);

    self->settings = g_settings_new("org.ldelossa.way-shell.window-manager");
    // simulate settings signals
    on_ignored_toplevels_app_ids_changed(self->settings,
                                         "ignored-toplevels-app-ids", self);
    on_ignored_toplevels_titles_changed(self->settings,
                                        "ignored-toplevels-titles", self);

    // add registry listener
    wl_registry_add_listener(self->registry, &registry_listener, self);

    // add wayland fd to event loop
    g_unix_fd_add(self->fd, G_IO_IN | G_IO_ERR | G_IO_HUP, on_fd_read, self);

    // issue a round-trip so we block until all globals are all bound and
    // listeners are registered.
    wl_display_flush(self->display);

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

void wayland_wlr_foreign_toplevel_activate(
    WaylandService *self, WaylandWLRForeignTopLevel *toplevel) {
    g_debug("wayland_service.c:wayland_wlr_foreign_toplevel_activate()");

    // ensure toplevel exists
    if (!g_hash_table_contains(self->toplevels, toplevel->toplevel)) return;

    // get first seat, very likely there is only one.
    GList *seats = g_hash_table_get_values(self->seats);
    WaylandSeat *seat = (WaylandSeat *)seats->data;
    if (!seat) {
        g_debug(
            "wayland_service.c:wayland_wlr_foreign_toplevel_activate(): "
            "no seat found");
    }

    zwlr_foreign_toplevel_handle_v1_activate(toplevel->toplevel, seat->seat);
    // flush
    wl_display_flush(self->display);
}

void wayland_wlr_foreign_toplevel_close(WaylandService *self,
                                        WaylandWLRForeignTopLevel *toplevel) {
    g_debug("wayland_service.c:wayland_wlr_foreign_toplevel_close()");
    zwlr_foreign_toplevel_handle_v1_close(toplevel->toplevel);
    wl_display_flush(self->display);
}

void wayland_wlr_foreign_toplevel_maximize(
    WaylandService *self, WaylandWLRForeignTopLevel *toplevel) {
    g_debug("wayland_service.c:wayland_wlr_foreign_toplevel_maximize()");
    zwlr_foreign_toplevel_handle_v1_set_maximized(toplevel->toplevel);
    wl_display_flush(self->display);
}

void wayland_wlr_shortcuts_inhibitor_create(WaylandService *self,
                                            GtkWidget *widget) {
    g_debug(
        "wayland_service.c:wayland_wlr_shortcuts_inhibitor_create() called");

    if (self->shorcuts_inhibited_toplevel) return;

    GtkNative *gdk_native = gtk_widget_get_native(widget);
    if (!gdk_native) {
        g_critical(
            "wayland_service.c:wayland_wlr_shortcuts_inhibitor_create(): "
            "gtk_widget_get_native() returned NULL");
        return;
    }

    GdkSurface *gdk_surface = gtk_native_get_surface(gdk_native);
    if (!gdk_surface) {
        g_critical(
            "wayland_service.c:wayland_wlr_shortcuts_inhibitor_create(): "
            "gtk_native_get_surface() returned NULL");
        return;
    }

    if (!GDK_IS_TOPLEVEL(gdk_surface)) {
        return;
    }

    g_debug(
        "wayland_service.c:wayland_wlr_shortcuts_inhibitor_create(): "
        "inhibiting system shortcuts");

    GdkToplevel *toplevel = GDK_TOPLEVEL(gdk_surface);
    gdk_toplevel_inhibit_system_shortcuts(toplevel, NULL);
    self->shortcuts_inhitibed = true;
    self->shorcuts_inhibited_toplevel = toplevel;
}

void wayland_wlr_shortcuts_inhibitor_destroy(WaylandService *self) {
    g_debug(
        "wayland_service.c:wayland_wlr_shortcuts_inhibitor_destroy() called");
    if (!self->shorcuts_inhibited_toplevel) return;

    g_debug(
        "wayland_service.c:wayland_wlr_shortcuts_inhibitor_destroy(): "
        "restoring system shortcuts");

    gdk_toplevel_restore_system_shortcuts(self->shorcuts_inhibited_toplevel);
}

static void wayland_wlr_gamma_control_apply(WaylandService *self,
                                            WaylandWLRGammaControl *ctrl) {
    g_debug("wayland_service.c:wayland_wlr_gamma_control_apply() called");
    // when we get here we must have the gamma ramp table size known
    if (ctrl->gamma_size == 0) return;

    // get the total size needed for the gamma table.
    size_t table_size = ctrl->gamma_size * 3 * sizeof(uint16_t);

    // create unique name for our shared memory gamma table.
    char shm_name[255];
    snprintf(shm_name, 255, "/way-shell-ephemeral-gamma-table-%d", rand());

    // we'll create a memory backed fd and share with Wayland.
    int fd = shm_open(shm_name, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        g_error(
            "wayland_service.c:wayland_wlr_gamma_control_apply() shm_open "
            "failed: %s",
            strerror(errno));
    }

    if (ftruncate(fd, table_size) < 0) {
        g_error(
            "wayland_service.c:wayland_wlr_gamma_control_apply() ftruncate "
            "failed: %s",
            strerror(errno));
    }

    uint16_t *table =
        mmap(NULL, table_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (table == MAP_FAILED) {
        g_error(
            "wayland_service.c:wayland_wlr_gamma_control_apply() mmap failed: "
            "%s",
            strerror(errno));
    }

    // fill gammma table.
    uint16_t *r = table;
    uint16_t *g = table + ctrl->gamma_size;
    uint16_t *b = table + (ctrl->gamma_size * 2);

    /* Initialize gamma ramps to pure state */
    for (int i = 0; i < ctrl->gamma_size; i++) {
        uint16_t value = (double)i / ctrl->gamma_size * (UINT16_MAX + 1);
        r[i] = value;
        g[i] = value;
        b[i] = value;
    }

    colorramp_fill(r, g, b, ctrl->gamma_size, ctrl->temperature);

    zwlr_gamma_control_v1_set_gamma(ctrl->control, fd);

    munmap(table, table_size);
    shm_unlink(shm_name);
    close(fd);

    ctrl->gamma_table_fd = fd;
}

static void zwlr_gamma_control_handle_size(
    void *data, struct zwlr_gamma_control_v1 *control, uint32_t size) {
    g_debug("wayland_service.c:zwlr_gamma_control_handle_size(): size: %d",
            size);

    WaylandService *self = (WaylandService *)data;

    WaylandWLRGammaControl *ctrl =
        g_hash_table_lookup(self->gamma_controllers, control);
    if (!ctrl) return;

    ctrl->gamma_size = size;

    if (self->gamma_control_enabled) {
        wayland_wlr_gamma_control_apply(self, ctrl);
    }

    wl_display_flush(self->display);

    g_debug(
        "wayland_service.c:zwlr_gamma_control_handle_size(): ctrl->gamma_size: "
        "%d",
        ctrl->gamma_size);
}

static void zlwr_gamma_control_handle_failed(
    void *data, struct zwlr_gamma_control_v1 *ctrl) {
    g_debug("wayland_service.c:zlwr_gamma_control_handle_failed(): failed");
    WaylandService *self = (WaylandService *)data;
    g_hash_table_remove(self->gamma_controllers, ctrl);
    zwlr_gamma_control_v1_destroy(ctrl);
    wl_display_flush(self->display);
}

static const struct zwlr_gamma_control_v1_listener gamma_control_listener = {
    .gamma_size = zwlr_gamma_control_handle_size,
    .failed = zlwr_gamma_control_handle_failed,
};

void wayland_wlr_bluelight_filter(WaylandService *self, double temperature) {
    g_debug("wayland_service.c:wayland_wlr_bluelight_filter(): intensity: %f",
            temperature);

    if (self->gamma_control_enabled) {
        for (GList *l = g_hash_table_get_values(self->gamma_controllers); l;
             l = l->next) {
            WaylandWLRGammaControl *ctrl = l->data;
            ctrl->temperature = temperature;
            wayland_wlr_gamma_control_apply(self, ctrl);
        }
    }

    self->gamma_control_enabled = true;
    self->temperature = temperature;

    // iterate over wl_outputs
    GList *outputs = g_hash_table_get_values(self->outputs);
    for (GList *l = outputs; l; l = l->next) {
        WaylandOutput *output = l->data;

        WaylandWLRGammaControl *ctrl =
            g_malloc0(sizeof(WaylandWLRGammaControl));

        ctrl->header.type = WLR_GAMMA_CONTROL;

        ctrl->output = output->output;

        ctrl->temperature = temperature;

        ctrl->gamma_size = 0;

        ctrl->gamma_table_fd = -1;

        ctrl->control = zwlr_gamma_control_manager_v1_get_gamma_control(
            self->gamma_control_manager, output->output);

        // add listener
        zwlr_gamma_control_v1_add_listener(ctrl->control,
                                           &gamma_control_listener, self);

        g_hash_table_insert(self->gamma_controllers, ctrl->control, ctrl);
    }

    wl_display_flush(self->display);

    g_signal_emit(self, service_signals[gamma_control_enabled], 0);
}

void wayland_wlr_bluelight_filter_destroy(WaylandService *self) {
    if (!self->gamma_control_enabled) return;

    for (GList *l = g_hash_table_get_values(self->gamma_controllers); l;
         l = l->next) {
        WaylandWLRGammaControl *ctrl = l->data;
        zwlr_gamma_control_v1_destroy(ctrl->control);
    }

    g_hash_table_remove_all(self->gamma_controllers);

    wl_display_flush(self->display);

    self->gamma_control_enabled = false;

    g_signal_emit(self, service_signals[gamma_control_disabled], 0);
}

gboolean wayland_wlr_gamma_control_enabled(WaylandService *self) {
    return self->gamma_control_enabled;
}
