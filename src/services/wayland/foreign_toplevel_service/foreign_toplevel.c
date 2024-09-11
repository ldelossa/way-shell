#include "foreign_toplevel.h"

#include <adwaita.h>

#include "../wlr-foreign-toplevel-management-unstable-v1.h"
#include "foreign_toplevel.h"

static WaylandForeignToplevelService *global = NULL;

enum signals { top_level_changed, top_level_removed, signals_n };

struct _WaylandForeignToplevelService {
    GObject parent_instance;
    WaylandCoreService *core;
    struct zwlr_foreign_toplevel_manager_v1 *mgr;
    // WaylandWLRForeignTopLevel structs mapped by
    // zwlr_foreign_top_level_handle_v1 structs
    GHashTable *toplevels;
    // Monitors org.ldelossa.way-shell.window-manager.ignored-toplevels-app-ids
    // and org.ldelossa.way-shell.window-manager.ignored-toplevels-titles
    GSettings *settings;
    GHashTable *ignored_toplevel_app_ids;
    GHashTable *ignored_toplevel_titles;
};

static guint service_signals[signals_n] = {0};

G_DEFINE_TYPE(WaylandForeignToplevelService, wayland_foreign_toplevel_service,
              G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init methods for this GObject
static void wayland_foreign_toplevel_service_dispose(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(wayland_foreign_toplevel_service_parent_class)
        ->dispose(gobject);
};

static void wayland_foreign_toplevel_service_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(wayland_foreign_toplevel_service_parent_class)
        ->finalize(gobject);
};

static void wayland_foreign_toplevel_service_class_init(
    WaylandForeignToplevelServiceClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = wayland_foreign_toplevel_service_dispose;
    object_class->finalize = wayland_foreign_toplevel_service_finalize;

    service_signals[top_level_changed] = g_signal_new(
        "top-level-changed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_HASH_TABLE, G_TYPE_POINTER);

    service_signals[top_level_removed] = g_signal_new(
        "top-level-removed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);
};

static void on_ignored_toplevels_app_ids_changed(
    GSettings *settings, gchar *key, WaylandForeignToplevelService *self) {
    g_debug("foreign_toplevel.c:on_ignored_toplevels_app_ids_changed(): key: %s",
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

static void on_ignored_toplevels_titles_changed(
    GSettings *settings, gchar *key, WaylandForeignToplevelService *self) {
    g_debug("foreign_toplevel.c:on_ignored_toplevels_titles_changed(): key: %s",
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

static void wayland_foreign_toplevel_service_init(
    WaylandForeignToplevelService *self) {
    self->toplevels = g_hash_table_new(g_direct_hash, g_direct_equal);
    self->ignored_toplevel_app_ids = g_hash_table_new(g_str_hash, g_str_equal);
    self->ignored_toplevel_titles = g_hash_table_new(g_str_hash, g_str_equal);

    self->settings = g_settings_new("org.ldelossa.way-shell.window-manager");
    on_ignored_toplevels_app_ids_changed(self->settings,
                                         "ignored-toplevels-app-ids", self);
    on_ignored_toplevels_titles_changed(self->settings,
                                        "ignored-toplevels-titles", self);

    g_signal_connect(self->settings, "changed::ignored-toplevels-app-ids",
                     G_CALLBACK(on_ignored_toplevels_app_ids_changed), self);
    g_signal_connect(self->settings, "changed::ignored-toplevels-titles",
                     G_CALLBACK(on_ignored_toplevels_titles_changed), self);
};

static const struct zwlr_foreign_toplevel_manager_v1_listener mgr_listener;

static const struct zwlr_foreign_toplevel_handle_v1_listener handle_listener;

int wayland_foreign_toplevel_service_global_init(
    WaylandCoreService *core, struct zwlr_foreign_toplevel_manager_v1 *mgr) {
    global = g_object_new(WAYLAND_FOREIGN_TOPLEVEL_SERVICE_TYPE, NULL);
    global->core = core;

    zwlr_foreign_toplevel_manager_v1_add_listener(mgr, &mgr_listener, global);

    return 0;
};

WaylandForeignToplevelService *wayland_foreign_toplevel_service_get_global() {
    return global;
};

GHashTable *wayland_foreign_toplevel_service_get_toplevels(
    WaylandForeignToplevelService *self) {
    return self->toplevels;
}

void wayland_foreign_toplevel_service_activate(
    WaylandForeignToplevelService *self, WaylandWLRForeignTopLevel *toplevel) {
    g_debug("wayland_foreign_toplevel_service_activate(): toplevel->app_id: %s",
            toplevel->app_id);

    // ensure toplevel exists
    if (!g_hash_table_contains(self->toplevels, toplevel->toplevel)) return;

    // get first seat, very likely there is only one.
    GList *seats =
        g_hash_table_get_values(wayland_core_service_get_seats(self->core));
    WaylandSeat *seat = (WaylandSeat *)seats->data;
    if (!seat) {
        g_debug(
            "foreign_toplevel.c:wayland_wlr_foreign_toplevel_activate(): "
            "no seat found");
    }

    zwlr_foreign_toplevel_handle_v1_activate(toplevel->toplevel, seat->seat);
    wl_display_flush(wayland_core_service_get_display(self->core));
}

void wayland_foreign_toplevel_service_close(
    WaylandForeignToplevelService *self, WaylandWLRForeignTopLevel *toplevel) {
    g_debug("wayland_foreign_toplevel_service_close(): toplevel->app_id: %s",
            toplevel->app_id);
    zwlr_foreign_toplevel_handle_v1_close(toplevel->toplevel);
    wl_display_flush(wayland_core_service_get_display(self->core));
}

void wayland_foreign_toplevel_service_maximize(
    WaylandForeignToplevelService *self, WaylandWLRForeignTopLevel *toplevel) {
    g_debug("wayland_foreign_toplevel_service_maximize(): toplevel->app_id: %s",
            toplevel->app_id);
    zwlr_foreign_toplevel_handle_v1_set_maximized(toplevel->toplevel);
    wl_display_flush(wayland_core_service_get_display(self->core));
}

//			 					//
// toplevel manager listener 	//
//			 					//
static void toplevel_mgr_new_toplevel(
    void *data, struct zwlr_foreign_toplevel_manager_v1 *mgr,
    struct zwlr_foreign_toplevel_handle_v1 *wayland_toplevel) {
    g_debug(
        "foreign_toplevel.c:toplevel_mgr_new_toplevel(): new toplevel handle");

    WaylandForeignToplevelService *self = (WaylandForeignToplevelService *)data;

    WaylandWLRForeignTopLevel *toplevel = g_new0(WaylandWLRForeignTopLevel, 1);
    toplevel->header.type = WL_REGISTRY;
    toplevel->toplevel = wayland_toplevel;

    g_hash_table_insert(self->toplevels, wayland_toplevel, toplevel);

    zwlr_foreign_toplevel_handle_v1_add_listener(wayland_toplevel,
                                                 &handle_listener, self);
}

static void toplevel_mgr_finished(
    void *data, struct zwlr_foreign_toplevel_manager_v1 *mgr) {
    g_debug(
        "foreign_toplevel.c:toplevel_mgr_finished(): toplevel manager "
        "finished");
    global = NULL;
}

static const struct zwlr_foreign_toplevel_manager_v1_listener mgr_listener = {
    .toplevel = toplevel_mgr_new_toplevel,
    .finished = toplevel_mgr_finished,
};

//			 					//
// toplevel handle listener 	//
//			 					//
static void toplevel_handle_app_id(
    void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
    const char *app_id) {
    g_debug("foreign_toplevel.c:toplevel_handle_app_id(): app_id: %s", app_id);

    WaylandForeignToplevelService *self = (WaylandForeignToplevelService *)data;
    WaylandWLRForeignTopLevel *top_level =
        g_hash_table_lookup(self->toplevels, handle);

    if (!top_level) {
        return;
    }

    top_level->app_id = g_strdup(app_id);

    g_debug("foreign_toplevel.c:toplevel_handle_app_id(): top_level->app_id: %s",
            top_level->app_id);
}

static void toplevel_handle_closed(
    void *data, struct zwlr_foreign_toplevel_handle_v1 *handle) {
    g_debug(
        "foreign_toplevel.c:toplevel_handle_closed(): toplevel handle closed");

    WaylandForeignToplevelService *self = (WaylandForeignToplevelService *)data;
    WaylandWLRForeignTopLevel *toplevel =
        g_hash_table_lookup(self->toplevels, handle);

    if (!toplevel) {
        return;
    }

    g_debug(
        "foreign_toplevel.c:toplevel_handle_closed(): toplevel->app_id: %s, "
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
    g_debug("foreign_toplevel.c:toplevel_handle_done(): toplevel handle done");

    WaylandForeignToplevelService *self = (WaylandForeignToplevelService *)data;
    WaylandWLRForeignTopLevel *toplevel =
        g_hash_table_lookup(self->toplevels, handle);

    if (!toplevel) {
        return;
    }

    // debug toplevel details
    g_debug(
        "foreign_toplevel.c:toplevel_handle_done(): toplevel->app_id: %s, "
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
        "foreign_toplevel.c:toplevel_handle_output_enter(): toplevel handle "
        "output enter");

    WaylandForeignToplevelService *self = (WaylandForeignToplevelService *)data;
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
        "foreign_toplevel.c:toplevel_handle_output_leave(): toplevel handle "
        "output leave");

    WaylandForeignToplevelService *self = (WaylandForeignToplevelService *)data;
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
        "foreign_toplevel.c:toplevel_handle_parent(): toplevel handle parent");
}

static void toplevel_handle_state(
    void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
    struct wl_array *state) {
    g_debug("foreign_toplevel.c:toplevel_handle_state(): toplevel handle state");

    WaylandForeignToplevelService *self = (WaylandForeignToplevelService *)data;
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

static void toplevel_handle_title(
    void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
    const char *title) {
    g_debug("foreign_toplevel.c:toplevel_handle_title(): title: %s", title);

    WaylandForeignToplevelService *self = (WaylandForeignToplevelService *)data;
    WaylandWLRForeignTopLevel *top_level =
        g_hash_table_lookup(self->toplevels, handle);

    if (!top_level) {
        return;
    }

    top_level->title = g_strdup(title);

    g_debug("foreign_toplevel.c:toplevel_handle_title(): top_level->title: %s",
            top_level->title);
}

static const struct zwlr_foreign_toplevel_handle_v1_listener handle_listener = {
    .app_id = toplevel_handle_app_id,
    .closed = toplevel_handle_closed,
    .done = toplevel_handle_done,
    .output_enter = toplevel_handle_output_enter,
    .output_leave = toplevel_handle_output_leave,
    .parent = toplevel_handle_parent,
    .state = toplevel_handle_state,
    .title = toplevel_handle_title,
};
