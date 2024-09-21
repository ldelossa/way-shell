#include "status_notifier_service.h"

#include <adwaita.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "../dbus_service.h"
#include "./status_notifier_host_dbus.h"
#include "./status_notifier_item_dbus.h"
#include "./status_notifier_watcher_dbus.h"
#include "gio/gdbusinterfaceskeleton.h"

static StatusNotifierService *global = NULL;

enum signals {
    status_notifier_item_added,
    status_notifier_item_removed,
    status_notifier_item_changed,
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
        g_signal_new("status-notifier-item-removed", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
                     G_TYPE_HASH_TABLE);
};

static void on_handle_register_async_cb(GObject *source_object,
                                        GAsyncResult *res, gpointer data) {
    StatusNotifierService *self = (StatusNotifierService *)data;

    g_debug("status_notifier_service.c:on_handle_register_async_cb() called");

    GError *error = NULL;
    DbusItemV0Gen *proxy = dbus_item_v0_gen_proxy_new_finish(res, &error);

    if (error) {
        g_critical(
            "status_notifier_service.c:on_handle_register_async_cb() failed to "
            "create proxy: %s",
            error->message);
        return;
    }
    if (!proxy) {
        g_critical(
            "status_notifier_service.c:on_handle_register_async_cb() failed to "
            "create proxy");
        return;
    }

    struct StatusNotifierItem *item =
        g_malloc(sizeof(struct StatusNotifierItem));
    item->proxy = proxy;
    item->bus_name = g_strdup(g_dbus_proxy_get_name(G_DBUS_PROXY(proxy)));
    item->obj_name =
        g_strdup(g_dbus_proxy_get_object_path(G_DBUS_PROXY(proxy)));

    g_hash_table_insert(self->items, item->bus_name, item);

    g_debug(
        "status_notifier_service.c:on_handle_register_async_cb() "
        "NotifierStatusItem for %s discovered",
        status_notifier_item_get_id(item));

    g_signal_emit(self, signals[status_notifier_item_added], 0, self->items,
                  item);
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
        obj_name = name;
        bus_name = g_dbus_method_invocation_get_sender(invocation);
    }

    g_debug(
        "status_notifier_service.c:on_handle_register_item() bus_name: %s, "
        "obj_name: %s",
        bus_name, obj_name);
    dbus_item_v0_gen_proxy_new(self->conn, G_DBUS_PROXY_FLAGS_NONE, bus_name,
                               obj_name, NULL, on_handle_register_async_cb,
                               self);

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

    DBUSService *dbus = dbus_service_get_global();
    g_signal_connect(dbus, "dbus-session-name-lost",
                     G_CALLBACK(on_session_name_lost), self);

    g_free(pid);
    g_free(bus_name);
};

StatusNotifierService *status_notifier_service_get_global() { return global; };

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
    return dbus_item_v0_gen_get_category(self->proxy);
}
const gchar *status_notifier_item_get_id(StatusNotifierItem *self) {
    return dbus_item_v0_gen_get_id(self->proxy);
}
const gchar *status_notifier_item_get_title(StatusNotifierItem *self) {
    return dbus_item_v0_gen_get_title(self->proxy);
}
const gchar *status_notifier_item_get_status(StatusNotifierItem *self) {
    return dbus_item_v0_gen_get_status(self->proxy);
}
const int status_notifier_item_get_window_id(StatusNotifierItem *self) {
    return dbus_item_v0_gen_get_window_id(self->proxy);
}
const gchar *status_notifier_item_get_icon_name(StatusNotifierItem *self) {
    return dbus_item_v0_gen_get_icon_name(self->proxy);
}
GdkPixbuf *status_notifier_item_get_icon_pixmap(StatusNotifierItem *self) {
    GVariant *icon_data = dbus_item_v0_gen_get_icon_pixmap(self->proxy);
    return pixbuf_from_icon_data(icon_data);
}
const gchar *status_notifier_item_get_overlay_icon_name(
    StatusNotifierItem *self) {
    return dbus_item_v0_gen_get_overlay_icon_name(self->proxy);
}
GdkPixbuf *status_notifier_item_get_overlay_icon_pixmap(
    StatusNotifierItem *self) {
    GVariant *icon_data = dbus_item_v0_gen_get_overlay_icon_pixmap(self->proxy);
    return pixbuf_from_icon_data(icon_data);
}
const gchar *status_notifier_item_get_attention_icon_name(
    StatusNotifierItem *self) {
    return dbus_item_v0_gen_get_attention_icon_name(self->proxy);
}
GdkPixbuf *status_notifier_item_get_attention_icon_pixmap(
    StatusNotifierItem *self) {
    GVariant *icon_data =
        dbus_item_v0_gen_get_attention_icon_pixmap(self->proxy);
    return pixbuf_from_icon_data(icon_data);
}
const gchar *status_notifier_item_get_attention_movie_name(
    StatusNotifierItem *self) {
    return dbus_item_v0_gen_get_attention_movie_name(self->proxy);
}
// TODO: get_tooltip
const gboolean status_notifier_item_get_item_is_menu(StatusNotifierItem *self) {
    return dbus_item_v0_gen_get_item_is_menu(self->proxy);
}
const gchar *status_notifier_item_get_menu(StatusNotifierItem *self) {
    return dbus_item_v0_gen_get_menu(self->proxy);
}

void status_notifier_item_free(StatusNotifierItem *self) {
    if (self->proxy) g_clear_object(&self->proxy);
    g_free(self->bus_name);
    g_free(self->obj_name);
    g_free(self);
}
