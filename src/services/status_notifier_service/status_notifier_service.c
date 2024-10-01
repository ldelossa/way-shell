#include "status_notifier_service.h"

#include <adwaita.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "../dbus_service.h"
#include "./status_notifier_host_dbus.h"
#include "./status_notifier_item_dbus.h"
#include "./status_notifier_watcher_dbus.h"
#include "dbusmenu_dbus.h"
#include "gio/gdbusinterfaceskeleton.h"
#include "libdbusmenu.h"

extern AdwApplication *gtk_app;

static StatusNotifierService *global = NULL;

static const char *property_names[] = {"accessible-desc",
                                       "children-display",
                                       "disposition",
                                       "enabled",
                                       "icon-data",
                                       "icon-name",
                                       "label",
                                       "shortcut",
                                       "toggle-type",
                                       "toggle-state",
                                       "type",
                                       "visible",
                                       NULL};

enum signals {
    status_notifier_item_added,
    status_notifier_item_removed,
    status_notifier_item_changed,
    status_notifier_item_properties_changed,
    status_notifier_item_menu_will_update,
    status_notifier_item_menu_updated,
    signals_n
};

struct _StatusNotifierService {
    GObject parent_instance;
    GDBusConnection *conn;
    DbusHostV0Gen *host;
    DbusWatcherV0Gen *watcher;
    DbusWatcherV0Gen *watcher_proxy;

    // Holds registered StatusNotifierItems
    GHashTable *items;

    gboolean enabled;
};
static guint signals[signals_n] = {0};
G_DEFINE_TYPE(StatusNotifierService, status_notifier_service, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init methods
static void status_notifier_service_dispose(GObject *gobject) {
    StatusNotifierService *self = STATUS_NOTIFIER_SERVICE(gobject);

    // Chain-up
    G_OBJECT_CLASS(status_notifier_service_parent_class)->dispose(gobject);
};

static void status_notifier_service_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(status_notifier_service_parent_class)->finalize(gobject);
};

static void status_notifier_service_class_init(
    StatusNotifierServiceClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = status_notifier_service_dispose;
    object_class->finalize = status_notifier_service_finalize;

    signals[status_notifier_item_added] =
        g_signal_new("status-notifier-item-added", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2,
                     G_TYPE_HASH_TABLE, G_TYPE_POINTER);

    signals[status_notifier_item_removed] =
        g_signal_new("status-notifier-item-removed", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2,
                     G_TYPE_HASH_TABLE, G_TYPE_POINTER);

    signals[status_notifier_item_changed] =
        g_signal_new("status-notifier-item-changed", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
                     G_TYPE_HASH_TABLE);

    signals[status_notifier_item_properties_changed] =
        g_signal_new("status-notifier-item-properties-changed",
                     G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0, NULL,
                     NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);

    signals[status_notifier_item_menu_will_update] =
        g_signal_new("status-notifier-item-menu-will-update",
                     G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0, NULL,
                     NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);

    signals[status_notifier_item_menu_updated] =
        g_signal_new("status-notifier-item-menu-updated",
                     G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0, NULL,
                     NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);
};

void on_menu_item_activate(GSimpleAction *action, GVariant *parameter,
                           gpointer data) {
    StatusNotifierService *self = (StatusNotifierService *)data;
    gchar *menu_bus_name;
    gint32 menu_item_id;

    g_debug("status_notifier_service.c:on_menu_item_activate() called");
    g_variant_get(parameter, "(si)", &menu_bus_name, &menu_item_id);

    if (!menu_bus_name) {
        g_warning(
            "status_notifier_service.c:on_menu_item_activate() menu_bus_name "
            "is NULL");
        return;
    }
    if (!menu_item_id) {
        g_warning(
            "status_notifier_service.c:on_menu_item_activate() menu_item_id is "
            "NULL");
        return;
    }

    g_debug(
        "status_notifier_service.c:on_menu_item_activate() menu_bus_name: %s, "
        "menu_item_id: %d",
        menu_bus_name, menu_item_id);

    StatusNotifierItem *item = g_hash_table_lookup(self->items, menu_bus_name);
    if (!item) {
        g_warning(
            "status_notifier_service.c:on_menu_item_activate() item not found: "
            "%s",
            menu_bus_name);
        return;
    }

    GError *error = NULL;
    GVariant *no_data = g_variant_new("v", g_variant_new_int32(0));

    dbus_dbusmenu_call_event_sync(item->menu_proxy, menu_item_id, "clicked",
                                  no_data, 1727549020, NULL, &error);
    if (error) {
        g_warning(
            "status_notifier_service.c:on_menu_item_activate() failed to send "
            "event: %s",
            error->message);
        g_error_free(error);
    }
}

static const GActionEntry action_entries[] = {
    {"item-clicked", on_menu_item_activate, "(si)", NULL, NULL},
};

static void set_item_actions(struct StatusNotifierItem *item,
                             StatusNotifierService *self) {
    item->action_group = G_ACTION_GROUP(g_simple_action_group_new());
    g_action_map_add_action_entries(G_ACTION_MAP(item->action_group),
                                    action_entries,
                                    G_N_ELEMENTS(action_entries), self);
}

static void update_menu_layout(DbusDbusmenu *menu, StatusNotifierItem *item) {
    g_debug("status_notifier_service.c:update_menu_layout() called");
    GVariant *layout;
    guint rev;
    GError *error = NULL;
    dbus_dbusmenu_call_get_layout_sync(item->menu_proxy, 0, -1, property_names,
                                       &rev, &layout, NULL, &error);
    if (error) {
        g_critical(
            "status_notifier_service.c:update_menu_layout() failed to get "
            "layout: %s",
            error->message);
        return;
    }

    libdbusmenu_parse_layout(layout, NULL, item);
}

static void on_property_update(DbusDbusmenu *menu, GVariant *arg_1,
                               GVariant *arg_2, StatusNotifierItem *item) {
    g_debug("status_notifier_service.c:on_property_update() called");
    g_signal_emit(item, signals[status_notifier_item_menu_will_update], 0,
                  item);
    update_menu_layout(menu, item);
    StatusNotifierService *s = status_notifier_service_get_global();
    g_signal_emit(s, signals[status_notifier_item_menu_updated], 0, item);
}

static void on_menu_layout_update(DbusDbusmenu *menu, guint arg_1, gint arg_2,
                                  StatusNotifierItem *item) {
    g_debug("status_notifier_service.c:on_menu_layout_update() called");
    g_signal_emit(item, signals[status_notifier_item_menu_will_update], 0,
                  item);
    update_menu_layout(menu, item);
    StatusNotifierService *s = status_notifier_service_get_global();
    g_signal_emit(s, signals[status_notifier_item_menu_updated], 0, item);
}

static void dbus_menu_parse_gmenu(StatusNotifierItem *item,
                                  StatusNotifierService *self) {
    GError *error = NULL;
    guint rev;
    GVariant *layout;

    g_debug(
        "status_notifier_service.c:dbus_menu_extract_g_menu() proxy name: %s",
        g_dbus_proxy_get_name(G_DBUS_PROXY(item->menu_proxy)));

    dbus_dbusmenu_call_get_layout_sync(item->menu_proxy, 0, -1, property_names,
                                       &rev, &layout, NULL, &error);
    if (error) {
        g_critical(
            "status_notifier_service.c:dbus_menu_extract_g_menu() failed to "
            "get layout: %s",
            error->message);
        return;
    }

    libdbusmenu_parse_layout(layout, NULL, item);

    set_item_actions(item, self);

    g_variant_unref(layout);
};

static void status_notifier_item_update(StatusNotifierItem *self,
                                        GVariant *props);

static void on_item_property_update_cb(GObject *source_object,
                                       GAsyncResult *res, gpointer data) {
    StatusNotifierItem *item = (StatusNotifierItem *)data;
    GVariant *properties = NULL;

    g_debug("status_notifier_service.c:on_item_property_update_cb() called");

    GError *error = NULL;
    properties =
        g_dbus_proxy_call_finish(G_DBUS_PROXY(source_object), res, &error);
    if (error) {
        g_critical(
            "status_notifier_service.c:on_item_property_update_cb() failed to "
            "get properties: %s",
            error->message);
        return;
    }

    g_debug(
        "status_notifier_service.c:on_item_property_update_cb() properties "
        "received");

    status_notifier_item_update(item, properties);
    g_variant_unref(properties);
}

static void on_item_property_update(DbusItemV0Gen *proxy,
                                    StatusNotifierItem *item) {
    g_debug("status_notifier_service.c:on_item_property_update() called");

    GVariant *argument = g_variant_new("(s)", "org.kde.StatusNotifierItem");
    g_dbus_proxy_call(G_DBUS_PROXY(item->proxy),
                      "org.freedesktop.DBus.Properties.GetAll", argument, 0,
                      300, NULL, on_item_property_update_cb, item);
}

static void on_handle_menu_async_cb(GObject *source_object, GAsyncResult *res,
                                    gpointer data) {
    StatusNotifierService *self = (StatusNotifierService *)data;
    GError *error = NULL;

    g_debug("status_notifier_service.c:on_handle_menu_async_cb() called");

    DbusDbusmenu *proxy = dbus_dbusmenu_proxy_new_finish(res, &error);
    if (error) {
        g_critical(
            "status_notifier_service.c:on_handle_menu_async_cb() failed to "
            "create proxy: %s",
            error->message);
        return;
    }

    StatusNotifierItem *item = g_hash_table_lookup(
        self->items, g_dbus_proxy_get_name(G_DBUS_PROXY(proxy)));

    if (!item) return;

    item->menu_proxy = proxy;
    dbus_menu_parse_gmenu(item, self);

    g_signal_connect(item->menu_proxy, "layout-updated",
                     G_CALLBACK(on_menu_layout_update), item);
    g_signal_connect(item->menu_proxy, "items-properties-updated",
                     G_CALLBACK(on_property_update), item);

    g_debug(
        "status_notifier_service.c:on_handle_menu_async_cb() "
        "NotifierStatusItem for %s discovered",
        status_notifier_item_get_id(item));

    g_signal_emit(self, signals[status_notifier_item_added], 0, self->items,
                  item);
}

static void on_session_name_lost(DBUSService *dbus, GHashTable *session_names,
                                 gchar *name, StatusNotifierService *self);

static void on_handle_register_async_cb(GObject *source_object,
                                        GAsyncResult *res, gpointer data) {
    StatusNotifierItem *item = (StatusNotifierItem *)data;
    StatusNotifierService *self = status_notifier_service_get_global();

    g_debug("status_notifier_service.c:on_handle_register_async_cb() called");

    GError *error = NULL;
    DbusItemV0Gen *proxy = dbus_item_v0_gen_proxy_new_finish(res, &error);
    if (error) {
        g_critical(
            "status_notifier_service.c:on_handle_register_async_cb() failed to "
            "create proxy: %s",
            error->message);

        // cleanup, since we told DBus we registered this item already...
        on_session_name_lost(NULL, NULL, item->bus_name, self);

        return;
    }
    if (!proxy) {
        g_critical(
            "status_notifier_service.c:on_handle_register_async_cb() failed to "
            "create proxy");

        // cleanup, since we told DBus we registered this item already...
        on_session_name_lost(NULL, NULL, item->bus_name, self);

        return;
    }

    // finish StatutNotifierItem init, now that we have a proxy...
    status_notifier_item_init(item, proxy);

    // connect to signals which require us to update properties.
    g_signal_connect(item->proxy, "new-icon",
                     G_CALLBACK(on_item_property_update), item);
    g_signal_connect(item->proxy, "new-attention-icon",
                     G_CALLBACK(on_item_property_update), item);
    g_signal_connect(item->proxy, "new-overlay-icon",
                     G_CALLBACK(on_item_property_update), item);
    g_signal_connect(item->proxy, "new-title",
                     G_CALLBACK(on_item_property_update), item);
    g_signal_connect(item->proxy, "new-status",
                     G_CALLBACK(on_item_property_update), item);

    // if a menu exists, we'll grab a proxy to it and emit our added signal
    // after we attach it to our StatusNotifierItem.
    if (status_notifier_item_get_item_is_menu(item) ||
        strlen(status_notifier_item_get_menu(item)) > 0) {
        g_debug(
            "status_notifier_service.c:on_handle_register_async_cb() "
            "StatusNotifierItem at %s:%s has Menu at %s",
            item->bus_name, item->obj_name,
            status_notifier_item_get_menu(item));
        g_hash_table_insert(self->items, item->bus_name, item);
        dbus_dbusmenu_proxy_new(self->conn, 0, item->bus_name,
                                status_notifier_item_get_menu(item), NULL,
                                on_handle_menu_async_cb, self);
        return;
    }

    // trigger our own internal signals.
    g_signal_emit(self, signals[status_notifier_item_added], 0, self->items,
                  item);
    g_debug(
        "status_notifier_service.c:on_handle_register_async_cb() "
        "NotifierStatusItem for %s discovered",
        status_notifier_item_get_id(item));
}

gboolean on_handle_register_item(DbusWatcherV0Gen *watcher,
                                 GDBusMethodInvocation *invocation, gchar *name,
                                 StatusNotifierService *self) {
    g_debug("status_notifier_service.c:on_handle_register_item() called: %s",
            name);

    gchar *obj_name = "/StatusNotifierItem";
    const gchar *bus_name = name;

    // check if bus_name contains forward slashes, its then an object and we
    // we have to find which bus name sent it.
    if (strchr(name, '/') != NULL) {
        const gchar *sender = g_dbus_method_invocation_get_sender(invocation);
        if (!sender) {
            g_critical(
                "status_notifier_service.c:on_handle_register_item() "
                "object path sent but sender is NULL");
            return TRUE;
        }
        obj_name = name;
        bus_name = sender;
    }

    g_debug(
        "status_notifier_service.c:on_handle_register_item() bus_name: %s, "
        "obj_name: %s",
        bus_name, obj_name);

    struct StatusNotifierItem *item =
        g_malloc0(sizeof(struct StatusNotifierItem));
    item->register_service_name = g_strdup(name);
    item->bus_name = g_strdup(g_dbus_method_invocation_get_sender(invocation));

    // register the item, we'll finish init in the proxy init cb which follows.
    g_hash_table_insert(self->items, item->bus_name, item);
    dbus_watcher_v0_gen_complete_register_item(watcher, invocation);
    dbus_watcher_v0_gen_emit_item_registered(watcher, name);

    dbus_item_v0_gen_proxy_new(self->conn, G_DBUS_PROXY_FLAGS_NONE, bus_name,
                               obj_name, NULL, on_handle_register_async_cb,
                               item);

    return TRUE;
}

static void on_status_notifier_host_name_acquired(GDBusConnection *conn,
                                                  const gchar *name,
                                                  gpointer user_data) {
    StatusNotifierService *self = STATUS_NOTIFIER_SERVICE(user_data);
    gboolean b = false;
    GError *error = NULL;

    char *pid = g_strdup_printf("%d", getpid());
    char *obj_name = g_strdup_printf("/StatusNotifierHost/%s", pid);
    b = g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(self->host),
                                         self->conn, obj_name, &error);
    if (!b)
        g_error(
            "status_notifier_service.c:export_notifications_dbus() failed to "
            "export dbus interface: %s",
            error->message);

    g_free(pid);
    g_free(obj_name);
}

static void connect_watcher_handlers(StatusNotifierService *self) {
    g_signal_connect(self->watcher, "handle-register-item",
                     G_CALLBACK(on_handle_register_item), self);
}

static void on_status_notifier_watcher_name_acquired(GDBusConnection *conn,
                                                     const gchar *name,
                                                     gpointer user_data) {
    StatusNotifierService *self = STATUS_NOTIFIER_SERVICE(user_data);
    gboolean b = false;
    GError *error = NULL;

    g_debug(
        "status_notifier_service.c:on_status_notifier_watcher_name_acquired() "
        "called");

    connect_watcher_handlers(self);

    b = g_dbus_interface_skeleton_export(
        G_DBUS_INTERFACE_SKELETON(self->watcher), self->conn,
        "/StatusNotifierWatcher", &error);

    if (!b)
        g_error(
            "status_notifier_service.c:export_notifications_dbus() failed to "
            "export dbus interface: %s",
            error->message);
}

static void on_status_notifier_host_name_lost(GDBusConnection *conn,
                                              const gchar *name,
                                              gpointer user_data) {
    StatusNotifierService *self = STATUS_NOTIFIER_SERVICE(user_data);
    g_error("status_notifier_service.c:on_name_lost() lost name %s", name);
};

static void on_session_name_lost(DBUSService *dbus, GHashTable *session_names,
                                 gchar *name, StatusNotifierService *self) {
    g_debug("status_notifier_service.c:on_session_name_lost() called");

    struct StatusNotifierItem *item = g_hash_table_lookup(self->items, name);
    if (!item) return;

    g_debug(
        "status_notifier_service.c:on_session_name_lost() removing proxy: %s",
        name);

    g_signal_emit(self, signals[status_notifier_item_removed], 0, self->items,
                  item);

    g_hash_table_remove(self->items, name);

    dbus_watcher_v0_gen_emit_item_unregistered(self->watcher,
                                               item->register_service_name);

    status_notifier_item_free(item);
}

static void status_notifier_service_dbus_connect(StatusNotifierService *self) {
    DBUSService *dbus = dbus_service_get_global();
    self->conn = dbus_service_get_session_bus(dbus);
}

static void status_notifier_service_init(StatusNotifierService *self) {
    g_debug("status_notifier_service.c:status_notifier_service_init() called");

    self->items = g_hash_table_new(g_str_hash, g_str_equal);
    self->enabled = false;

    status_notifier_service_dbus_connect(self);

    // StatusNotifierHost
    self->host = dbus_host_v0_gen_skeleton_new();
    char *pid = g_strdup_printf("%d", getpid());
    char *bus_name = g_strdup_printf("org.kde.StatusNotifierHost-%s", pid);
    g_bus_own_name_on_connection(self->conn, bus_name,
                                 G_BUS_NAME_OWNER_FLAGS_NONE,
                                 on_status_notifier_host_name_acquired,
                                 on_status_notifier_host_name_lost, self, NULL);

    // StatusNotifierWatcher
    self->watcher = dbus_watcher_v0_gen_skeleton_new();
    g_bus_own_name_on_connection(self->conn, "org.kde.StatusNotifierWatcher",
                                 G_BUS_NAME_OWNER_FLAGS_NONE,
                                 on_status_notifier_watcher_name_acquired,
                                 on_status_notifier_host_name_lost, self, NULL);

    dbus_watcher_v0_gen_set_is_host_registered(self->watcher, TRUE);
    dbus_watcher_v0_gen_emit_host_registered(self->watcher);

    DBUSService *dbus = dbus_service_get_global();
    g_signal_connect(dbus, "dbus-session-name-lost",
                     G_CALLBACK(on_session_name_lost), self);

    g_free(pid);
    g_free(bus_name);
};

StatusNotifierService *status_notifier_service_get_global() { return global; };

GHashTable *status_notifier_service_get_items(StatusNotifierService *self) {
    return self->items;
}

int status_notifier_service_global_init() {
    global = g_object_new(NOTIFICATIONS_SERVICE_TYPE, NULL);
    return 0;
}

//							//
// NotifierItem Methods		//
//							//

// ripped from Waybar, thank you!!
GdkPixbuf *pixbuf_from_icon_data(GVariant *icon_data) {
    if (!icon_data) {
        return NULL;
    }

    GVariantIter *it;
    g_variant_get(icon_data, "a(iiay)", &it);
    if (!it) return NULL;

    GVariant *val;
    gint lwidth = 0;
    gint lheight = 0;
    gint width;
    gint height;
    guchar *array = NULL;
    while (g_variant_iter_loop(it, "(ii@ay)", &width, &height, &val)) {
        if (width > 0 && height > 0 && val &&
            width * height > lwidth * lheight) {
            int size = g_variant_get_size(val);
            /* Sanity check */
            if (size == 4U * width * height) {
                /* Find the largest image */
                gconstpointer data = g_variant_get_data(val);
                if (data) {
                    if (array) {
                        g_free(array);
                    }
                    array = g_memdup2(data, size);
                    lwidth = width;
                    lheight = height;
                }
            }
        }
    }
    g_variant_iter_free(it);
    if (array) {
        /* argb to rgba */
        for (uint32_t i = 0; i < 4U * lwidth * lheight; i += 4) {
            guchar alpha = array[i];
            array[i] = array[i + 1];
            array[i + 1] = array[i + 2];
            array[i + 2] = array[i + 3];
            array[i + 3] = alpha;
        }
        return gdk_pixbuf_new_from_data(array, GDK_COLORSPACE_RGB, TRUE, 8,
                                        lwidth, lheight, 4 * lwidth, NULL,
                                        NULL);
    }
    return NULL;
}

const gchar *status_notifier_item_get_category(StatusNotifierItem *self) {
    return self->category;
}
const gchar *status_notifier_item_get_id(StatusNotifierItem *self) {
    return self->id;
}
const gchar *status_notifier_item_get_title(StatusNotifierItem *self) {
    return self->title;
}
const gchar *status_notifier_item_get_status(StatusNotifierItem *self) {
    return self->status;
}
const int status_notifier_item_get_window_id(StatusNotifierItem *self) {
    return self->window_id;
}
const gchar *status_notifier_item_get_icon_name(StatusNotifierItem *self) {
    return self->icon_name;
}
GdkPixbuf *status_notifier_item_get_icon_pixmap(StatusNotifierItem *self) {
    return self->icon_pixmap;
}
const gchar *status_notifier_item_get_overlay_icon_name(
    StatusNotifierItem *self) {
    return self->overlay_icon_name;
}
GdkPixbuf *status_notifier_item_get_overlay_icon_pixmap(
    StatusNotifierItem *self) {
    return self->overlay_icon_pixmap;
}
const gchar *status_notifier_item_get_attention_icon_name(
    StatusNotifierItem *self) {
    return self->attention_icon_name;
}
GdkPixbuf *status_notifier_item_get_attention_icon_pixmap(
    StatusNotifierItem *self) {
    return self->attention_icon_pixmap;
}
const gchar *status_notifier_item_get_attention_movie_name(
    StatusNotifierItem *self) {
    return self->attention_movie_name;
}
// TODO: get_tooltip
const gboolean status_notifier_item_get_item_is_menu(StatusNotifierItem *self) {
    return dbus_item_v0_gen_get_item_is_menu(self->proxy);
}
const gchar *status_notifier_item_get_menu(StatusNotifierItem *self) {
    return dbus_item_v0_gen_get_menu(self->proxy);
}

DbusItemV0Gen *status_notifier_item_get_proxy(StatusNotifierItem *self) {
    return self->proxy;
}

void status_notifier_item_about_to_show(StatusNotifierItem *self,
                                        gint32 menu_item_id) {
    g_debug(
        "status_notifier_service.c:status_notifier_item_about_to_show() "
        "called");
    if (!self) {
        return;
    }

    GError *error = NULL;
    gboolean need_update = FALSE;

    dbus_dbusmenu_call_about_to_show_sync(self->menu_proxy, menu_item_id,
                                          &need_update, NULL, &error);
    if (error) {
        g_warning(
            "status_notifier_service.c:on_menu_about_to_show() failed to send "
            "event: %s",
            error->message);
        g_error_free(error);
    }

    if (need_update) {
        update_menu_layout(self->menu_proxy, self);
    }
}

GdkPixbuf *icon_pixbuf_from_icon_theme(gchar *icon_name,
                                       const gchar *icon_theme_path) {
    if (!icon_name || !icon_theme_path) {
        return NULL;
    }

    gchar *path_to_icon = NULL;

    // check if `icon_name` has an extension, any extension at all
    if (strchr(icon_name, '.') != NULL) {
        path_to_icon = g_strdup_printf("%s/%s", icon_theme_path, icon_name);
    } else {
        // we don't have a file listed, so lets search for it...
        GDir *dir = g_dir_open(icon_theme_path, 0, NULL);
        const gchar *file_name;
        while ((file_name = g_dir_read_name(dir)) != NULL) {
            if (g_str_has_prefix(file_name, icon_name) &&
                (g_str_has_suffix(file_name, ".png") ||
                 g_str_has_suffix(file_name, ".svg") ||
                 g_str_has_suffix(file_name, ".xpm") ||
                 g_str_has_suffix(file_name, ".ico"))) {
                path_to_icon =
                    g_strdup_printf("%s/%s", icon_theme_path, file_name);
                break;
            }
        }
    }

    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(path_to_icon, NULL);
    g_free(path_to_icon);
    return pixbuf;
}

static void status_notifier_item_update(StatusNotifierItem *self,
                                        GVariant *props) {
    g_debug("status_notifier_service.c:status_notifier_item_update() called");
    GVariant *array = g_variant_get_child_value(props, 0);
    GVariantIter iter;
    g_variant_iter_init(&iter, array);
    GVariant *value;
    const gchar *key;
    while (g_variant_iter_next(&iter, "{sv}", &key, &value)) {
        g_debug(
            "status_notifier_service.c:status_notifier_item_update() key: %s",
            key);
        // Category
        if (g_strcmp0(key, "Category") == 0) {
            if (self->category) g_free(self->category);
            self->category = g_variant_dup_string(value, NULL);
        }
        // ID
        else if (g_strcmp0(key, "Id") == 0) {
            if (self->id) g_free(self->id);
            self->id = g_variant_dup_string(value, NULL);
        }
        // Title
        else if (g_strcmp0(key, "Title") == 0) {
            if (self->title) g_free(self->title);
            self->title = g_variant_dup_string(value, NULL);
        }
        // Status
        else if (g_strcmp0(key, "Status") == 0) {
            if (self->status) g_free(self->status);
            self->status = g_variant_dup_string(value, NULL);
        }
        // WindowID
        else if (g_strcmp0(key, "WindowId") == 0) {
            self->window_id = g_variant_get_int32(value);
        }
        // IconThemePath
        else if (g_strcmp0(key, "IconThemePath") == 0) {
            if (self->icon_theme_path) g_free(self->icon_theme_path);
            self->icon_theme_path = g_variant_dup_string(value, NULL);
        }
        // IconName
        else if (g_strcmp0(key, "IconName") == 0) {
            if (self->icon_name) g_free(self->icon_name);
            self->icon_name = g_variant_dup_string(value, NULL);
        }
        // IconPixmap
        else if (g_strcmp0(key, "IconPixmap") == 0) {
            if (self->icon_pixmap) g_object_unref(self->icon_pixmap);
            self->icon_pixmap = pixbuf_from_icon_data(value);
        }
        // OverlayIconName
        else if (g_strcmp0(key, "OverlayIconName") == 0) {
            if (self->overlay_icon_name) g_free(self->overlay_icon_name);
            self->overlay_icon_name = g_variant_dup_string(value, NULL);
        }
        // OverlayIconPixmap
        else if (g_strcmp0(key, "OverlayIconPixmap") == 0) {
            if (self->overlay_icon_pixmap)
                g_object_unref(self->overlay_icon_pixmap);
            self->overlay_icon_pixmap = pixbuf_from_icon_data(value);
        }
        // AttentionIconName
        else if (g_strcmp0(key, "AttentionIconName") == 0) {
            if (self->attention_icon_name) g_free(self->attention_icon_name);
            self->attention_icon_name = g_variant_dup_string(value, NULL);
        }
        // AttentionIconPixmap
        else if (g_strcmp0(key, "AttentionIconPixmap") == 0) {
            if (self->attention_icon_pixmap)
                g_object_unref(self->attention_icon_pixmap);
            self->attention_icon_pixmap = pixbuf_from_icon_data(value);
        }
        // AttentionMovieName
        else if (g_strcmp0(key, "AttentionMovieName") == 0) {
            if (self->attention_movie_name) g_free(self->attention_movie_name);
            self->attention_movie_name = g_variant_dup_string(value, NULL);
        }
        // TODO: Tooltips
    }

    // if this update resulted in having both an icon theme path and an icon
    // name, load up the icon bits from the file.
    if (self->icon_theme_path && self->icon_name &&
        strlen(self->icon_name) > 0 && strlen(self->icon_theme_path) > 0) {
        if (self->icon_pixmap_from_theme) {
            g_object_unref(self->icon_pixmap_from_theme);
        }
        self->icon_pixmap_from_theme =
            icon_pixbuf_from_icon_theme(self->icon_name, self->icon_theme_path);
    }

    StatusNotifierService *s = status_notifier_service_get_global();
    g_signal_emit(s, signals[status_notifier_item_properties_changed], 0, self);
}

// initial settings come from the cached proxy, updates come from a call to
// GetAll for properties.
void *status_notifier_item_init(StatusNotifierItem *self,
                                DbusItemV0Gen *proxy) {
    self->proxy = proxy;
    self->obj_name =
        g_strdup(g_dbus_proxy_get_object_path(G_DBUS_PROXY(proxy)));
    self->menu_proxy = NULL;
    self->menu_model = NULL;

    self->category = g_strdup(dbus_item_v0_gen_get_category(proxy));
    self->id = g_strdup(dbus_item_v0_gen_get_id(proxy));
    self->title = g_strdup(dbus_item_v0_gen_get_title(proxy));
    self->status = g_strdup(dbus_item_v0_gen_get_status(proxy));
    self->window_id = dbus_item_v0_gen_get_window_id(proxy);
    self->icon_name = g_strdup(dbus_item_v0_gen_get_icon_name(proxy));
    self->icon_pixmap =
        pixbuf_from_icon_data(dbus_item_v0_gen_get_icon_pixmap(proxy));
    self->overlay_icon_name =
        g_strdup(dbus_item_v0_gen_get_overlay_icon_name(proxy));
    self->overlay_icon_pixmap =
        pixbuf_from_icon_data(dbus_item_v0_gen_get_icon_pixmap(proxy));
    self->attention_icon_name =
        g_strdup(dbus_item_v0_gen_get_attention_icon_name(proxy));
    self->attention_icon_pixmap = pixbuf_from_icon_data(
        dbus_item_v0_gen_get_attention_icon_pixmap(proxy));
    self->attention_movie_name =
        g_strdup(dbus_item_v0_gen_get_attention_movie_name(proxy));

    self->icon_theme_path =
        g_strdup(dbus_item_v0_gen_get_icon_theme_path(proxy));
    if (self->icon_theme_path && strlen(self->icon_theme_path) > 0) {
        self->icon_pixmap_from_theme =
            icon_pixbuf_from_icon_theme(self->icon_name, self->icon_theme_path);
    }

    return self;
}

void status_notifier_item_free(StatusNotifierItem *self) {
    g_debug("status_notifier_service.c:status_notifier_item_free() called");
    if (self->proxy) g_clear_object(&self->proxy);
    g_free(self->bus_name);
    g_free(self->obj_name);
    g_free(self->register_service_name);
    if (self->menu_proxy) {
        g_object_unref(self->menu_proxy);
        g_object_unref(self->menu_model);
    }
    if (self->icon_pixmap) g_object_unref(self->icon_pixmap);
    if (self->overlay_icon_pixmap) g_object_unref(self->overlay_icon_pixmap);
    if (self->attention_icon_pixmap)
        g_object_unref(self->attention_icon_pixmap);
    if (self->category) g_free(self->category);
    if (self->id) g_free(self->id);
    if (self->title) g_free(self->title);
    if (self->status) g_free(self->status);
    if (self->icon_name) g_free(self->icon_name);
    if (self->icon_theme_path) g_free(self->icon_theme_path);
    if (self->icon_pixmap_from_theme)
        g_object_unref(self->icon_pixmap_from_theme);
    if (self->overlay_icon_name) g_free(self->overlay_icon_name);
    if (self->attention_icon_name) g_free(self->attention_icon_name);
    if (self->attention_movie_name) g_free(self->attention_movie_name);
    g_free(self);
}
