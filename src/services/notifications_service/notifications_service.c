#include "notifications_service.h"

#include <adwaita.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "../dbus_service.h"
#include "gio/gdbusinterfaceskeleton.h"
#include "notifications_dbus.h"

void print_notification(const Notification *n) {
    g_debug("Notification:");
    g_debug("  app_name: %s", n->app_name ? n->app_name : "(null)");
    g_debug("  app_icon: %s", n->app_icon ? n->app_icon : "(null)");
    g_debug("  summary: %s", n->summary ? n->summary : "(null)");
    g_debug("  body: %s", n->body ? n->body : "(null)");
    g_debug("  category: %s", n->category ? n->category : "(null)");
    g_debug("  desktop_entry: %s",
            n->desktop_entry ? n->desktop_entry : "(null)");
    g_debug("  image_path: %s", n->image_path ? n->image_path : "(null)");
    g_debug("  id: %d", n->id);
    g_debug("  replaces_id: %u", n->replaces_id);
    g_debug("  expire_timeout: %d", n->expire_timeout);
    g_debug("  action_icons: %s", n->action_icons ? "true" : "false");
    g_debug("  resident: %s", n->resident ? "true" : "false");
    g_debug("  transient: %s", n->transient ? "true" : "false");
    g_debug("  urgency: %u", n->urgency);

    if (n->actions) {
        for (int i = 0; n->actions[i]; i++) {
            g_debug("  action[%d]: %s", i, n->actions[i]);
        }
    } else {
        g_debug("  actions: (null)");
    }

    if (n->img_data.data) {
        g_debug("  img_data:");
        g_debug("    width: %u", n->img_data.width);
        g_debug("    height: %u", n->img_data.height);
        g_debug("    rowstride: %u", n->img_data.rowstride);
        g_debug("    has_alpha: %s", n->img_data.has_alpha ? "true" : "false");
        g_debug("    bits_per_sample: %u", n->img_data.bits_per_sample);
        g_debug("    channels: %u", n->img_data.channels);
        // Print only the first 10 bytes of the data to avoid flooding the log
        if (n->img_data.data) {
            g_debug("    data: %X", n->img_data.data[0]);
        } else {
            g_debug("    data: (null)");
        }
    } else {
        g_debug("  img_data: (null)");
    }
}

static NotificationsService *global = NULL;

enum signals {
    notification_added,
    notification_closed,
    notification_changed,
    signals_n
};

static const char *caps[] = {"action-icons",    "actions",     "body",
                             "body-hyperlinks", "body-images", "body-markup",
                             "icon-multi",      "icon-static", "persistence"};

struct _NotificationsService {
    GObject parent_instance;
    DbusNotifications *dbus;
    GDBusConnection *conn;
    GPtrArray *notifications;
    GHashTable *internal_ids;
    uint32_t last_id;
    gboolean enabled;
};
static guint signals[signals_n] = {0};
G_DEFINE_TYPE(NotificationsService, notifications_service, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init methods
static void notifications_service_dispose(GObject *gobject) {
    NotificationsService *self = NOTIFICATIONS_SERVICE(gobject);

    // Chain-up
    G_OBJECT_CLASS(notifications_service_parent_class)->dispose(gobject);
};

static void notifications_service_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(notifications_service_parent_class)->finalize(gobject);
};

static void notifications_service_class_init(NotificationsServiceClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = notifications_service_dispose;
    object_class->finalize = notifications_service_finalize;

    // define notifications_changed signal
    signals[notification_added] =
        g_signal_new("notification-added", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 3,
                     G_TYPE_PTR_ARRAY, G_TYPE_UINT, G_TYPE_UINT);
    signals[notification_closed] =
        g_signal_new("notification-closed", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 3,
                     G_TYPE_PTR_ARRAY, G_TYPE_UINT, G_TYPE_UINT);
    signals[notification_changed] =
        g_signal_new("notification-changed", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
                     G_TYPE_PTR_ARRAY);
};

static void on_name_acquired(GDBusConnection *conn, const gchar *name,
                             gpointer user_data) {
    NotificationsService *self = NOTIFICATIONS_SERVICE(user_data);
    g_debug(
        "notifications_service.c:on_name_acquired() acquired name "
        "org.freedesktop.Notifications");
};

static void on_name_lost(GDBusConnection *conn, const gchar *name,
                         gpointer user_data) {
    NotificationsService *self = NOTIFICATIONS_SERVICE(user_data);
    g_error(
        "notifications_service.c:on_name_lost() lost name or unable to obtain "
        "name org.freedesktop.Notifications... "
        "Is another notification daemon running?");
};

static void notifications_service_dbus_connect(NotificationsService *self) {
    DBUSService *dbus = dbus_service_get_global();
    self->conn = dbus_service_get_session_bus(dbus);
}

static gboolean on_handle_get_server_information(
    DbusNotifications *dbus, GDBusMethodInvocation *invocation,
    NotificationsService *self) {
    g_debug(
        "notifications_service.c:on_handle_get_server_information() called");

    GVariant *ret =
        g_variant_new("(ssss)", "way-shell", "way-shell", "0.1", "1.2");

    g_dbus_method_invocation_return_value(invocation, ret);
    return TRUE;
}

static void parse_notify_hints(GVariant *hints, Notification *n) {
    if (!hints) return;

    GVariantIter *iter = NULL;
    GVariant *value = NULL;
    const char *key = NULL;

    g_variant_get(hints, "a{sv}", &iter);
    while (g_variant_iter_loop(iter, "{&sv}", &key, &value)) {
        // parse category
        if (g_strcmp0(key, "category") == 0) {
            n->category = g_variant_dup_string(value, NULL);
        }
        // parse action-icon
        if (g_strcmp0(key, "action-icons") == 0) {
            n->action_icons = g_variant_get_boolean(value);
        }
        // parse desktop-entry
        if (g_strcmp0(key, "desktop-entry") == 0) {
            n->desktop_entry = g_variant_dup_string(value, NULL);
        }
        // parse image data
        if (g_strcmp0(key, "image-data") == 0 ||
            g_strcmp0(key, "image_data") == 0 ||
            g_strcmp0(key, "icon_data") == 0) {
            gsize n_elements;
            g_variant_get(value, "(iiibii@ay)", &n->img_data.width,
                          &n->img_data.height, &n->img_data.rowstride,
                          &n->img_data.has_alpha, &n->img_data.bits_per_sample,
                          &n->img_data.channels, &value);
            n->img_data.data = (char *)g_variant_get_fixed_array(
                value, &n_elements, sizeof(guchar));
        }
        // parse image path
        if (g_strcmp0(key, "image-path") == 0 ||
            g_strcmp0(key, "image_path") == 0) {
            n->image_path = g_variant_dup_string(value, NULL);
        }
        // parse resident
        if (g_strcmp0(key, "resident") == 0) {
            n->resident = g_variant_get_boolean(value);
        }
        // parse transient
        if (g_strcmp0(key, "transient") == 0) {
            n->transient = g_variant_get_boolean(value);
        }
        // parse urgency
        if (g_strcmp0(key, "urgency") == 0) {
            n->urgency = g_variant_get_byte(value);
        }
    }

    return;
}

void notifications_service_send_notification(NotificationsService *self,
                                             Notification *n) {
    Notification *nn = g_malloc0(sizeof(Notification));
    nn->id = self->last_id++;
    nn->summary = g_strdup(n->summary);
    nn->body = g_strdup(n->body);
    nn->app_icon = g_strdup(n->app_icon);
    nn->urgency = n->urgency;
    nn->is_internal = true;

    g_hash_table_add(self->internal_ids, GUINT_TO_POINTER(nn->id));
    g_ptr_array_add(self->notifications, nn);
    // emit notifications change signal
    g_signal_emit(self, signals[notification_added], 0, self->notifications,
                  nn->id, (self->notifications->len - 1));
    g_signal_emit(self, signals[notification_changed], 0, self->notifications);
}

static gboolean on_handle_notify(DbusNotifications *dbus,
                                 GDBusMethodInvocation *invocation,
                                 const char *app_name, uint32_t replaces_id,
                                 const char *app_icon, const char *summary,
                                 const char *body, const char **actions,
                                 GVariant *hints, int32_t expire_timeout,
                                 NotificationsService *self) {
    g_debug("notifications_service.c:on_handle_notify() called");

    Notification *n = g_malloc0(sizeof(Notification));

    if (app_name) n->app_name = g_strdup(app_name);
    if (app_icon) n->app_icon = g_strdup(app_icon);
    if (summary) n->summary = g_strdup(summary);
    if (body) n->body = g_strdup(body);
    if (actions) n->actions = g_strdupv((char **)actions);

    // parse hints
    parse_notify_hints(hints, n);

    n->id = self->last_id++;
    n->replaces_id = replaces_id;
    n->expire_timeout = expire_timeout;
    n->created_on = g_date_time_new_now_local();

    // if we don't have an app name, we can try to resolve it from our desktop
    // entry.
    if (strlen(n->app_name) == 0 && strlen(n->desktop_entry) > 0) {
        g_free(n->app_name);
        n->app_name = g_strdup(n->desktop_entry);

        char *desktop_entry = g_strdup(n->desktop_entry);
        desktop_entry = g_strconcat(desktop_entry, ".desktop", NULL);

        GDesktopAppInfo *info = g_desktop_app_info_new(desktop_entry);
        if (info) {
            const char *name = g_desktop_app_info_get_string(info, "Name");
            if (name) {
                g_free(n->app_name);
                n->app_name = g_strdup(name);
            }
            g_object_unref(info);
        }

        g_free(desktop_entry);
    }

    g_ptr_array_add(self->notifications, n);

    GVariant *ret = g_variant_new("(u)", n->id);
    if (n->replaces_id > 0) {
        ret = g_variant_new("(u)", n->replaces_id);
    }

    // debug notification fields in a single g_debug call
    print_notification(n);

    g_dbus_method_invocation_return_value(invocation, ret);

    // emit notifications change signal
    g_signal_emit(self, signals[notification_added], 0, self->notifications,
                  n->id, (self->notifications->len - 1));
    g_signal_emit(self, signals[notification_changed], 0, self->notifications);

    return TRUE;
}

gboolean on_handle_get_capabilities(DbusNotifications *dbus,
                                    GDBusMethodInvocation *invocation,
                                    NotificationsService *self) {
    g_debug("notifications_service.c:on_handle_get_capabilities() called");

    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(as)"));

    g_variant_builder_open(builder, G_VARIANT_TYPE("as"));

    for (int i = 0; i < G_N_ELEMENTS(caps); i++) {
        g_variant_builder_add(builder, "s", caps[i]);
    }

    g_variant_builder_close(builder);

    GVariant *ret = g_variant_builder_end(builder);

    g_dbus_method_invocation_return_value(invocation, ret);
    return TRUE;
}

gboolean on_handle_close_notification(DbusNotifications *dbus,
                                      GDBusMethodInvocation *invocation,
                                      guint32 id, NotificationsService *self) {
    g_debug("notifications_service.c:on_handle_close_notification() called");
    notifications_service_closed_notification(
        self, id, NOTIFICATIONS_CLOSED_REASON_REQUESTED);

    GVariant *ret = g_variant_new("()");
    g_dbus_method_invocation_return_value(invocation, ret);
    return TRUE;
}

static void notifications_service_dbus_export_obj(NotificationsService *self) {
    gboolean b = false;
    GError *error = NULL;

    self->dbus = dbus_notifications_skeleton_new();

    g_debug("notifications_service.c:export_notifications_dbus() called");

    b = g_dbus_interface_skeleton_export(
        G_DBUS_INTERFACE_SKELETON(self->dbus), self->conn,
        "/org/freedesktop/Notifications", &error);
    if (!b)
        g_error(
            "notifications_service.c:export_notifications_dbus() failed to "
            "export dbus interface: %s",
            error->message);
}

void connect_handlers(NotificationsService *self) {
    g_signal_connect(self->dbus, "handle-get-capabilities",
                     G_CALLBACK(on_handle_get_capabilities), self);
    g_signal_connect(self->dbus, "handle-notify", G_CALLBACK(on_handle_notify),
                     self);
    g_signal_connect(self->dbus, "handle-get-server-information",
                     G_CALLBACK(on_handle_get_server_information), self);
    g_signal_connect(self->dbus, "handle-close-notification",
                     G_CALLBACK(on_handle_close_notification), self);
}

static void notifications_service_init(NotificationsService *self) {
    g_debug("notifications_service.c:notifications_service_init() called");

    self->enabled = false;

    notifications_service_dbus_connect(self);

    notifications_service_dbus_export_obj(self);

    connect_handlers(self);

    g_bus_own_name_on_connection(self->conn, "org.freedesktop.Notifications",
                                 G_BUS_NAME_OWNER_FLAGS_NONE, on_name_acquired,
                                 on_name_lost, self, NULL);

    self->notifications = g_ptr_array_new();

    self->internal_ids = g_hash_table_new(g_direct_hash, g_direct_equal);

    self->enabled = true;
};

static void free_notification(Notification *n) {
    if (n->app_name) g_free(n->app_name);
    if (n->app_icon) g_free(n->app_icon);
    if (n->summary) g_free(n->summary);
    if (n->body) g_free(n->body);
    if (n->actions) g_strfreev(n->actions);
    if (n->category) g_free(n->category);
    if (n->desktop_entry) g_free(n->desktop_entry);
    if (n->image_path) g_free(n->image_path);
    if (n->created_on) g_date_time_unref(n->created_on);
    g_free(n);
}

int notifications_service_closed_notification(
    NotificationsService *self, guint32 id,
    enum NotifcationsClosedReason reason) {
    Notification *n = NULL;
    guint32 index = 0;

    g_debug(
        "notifications_service.c:notification_service_close_notification() "
        "called");

    for (int i = 0; i < self->notifications->len; i++) {
        Notification *tmp = g_ptr_array_index(self->notifications, i);
        if (tmp->id == id) {
            n = tmp;
            index = i;
            break;
        }
    }
    if (!n) {
        g_warning(
            "notifications_service.c:notification_service_close_notification() "
            "notification not found in array");
        return -1;
    }

    // emit notification closed before we free memory, tells listeners to
    // jetison this notification.
    g_signal_emit(self, signals[notification_closed], 0, self->notifications,
                  n->id, index);

    g_ptr_array_remove_fast(self->notifications, n);
    free_notification(n);

    // if id is an internal id, remove it from internal ids set.
    // if not, notify dbus.
    if (g_hash_table_contains(self->internal_ids, GUINT_TO_POINTER(id))) {
        g_hash_table_remove(self->internal_ids, GUINT_TO_POINTER(id));
    } else {
        dbus_notifications_emit_notification_closed(self->dbus, id, reason);
    }

    g_signal_emit(self, signals[notification_changed], 0, self->notifications);

    return 0;
}

int notifications_service_invoke_action(NotificationsService *self, guint32 id,
                                        char *action_key) {
    g_debug(
        "notifications_service.c:notification_service_invoke_action() "
        "called");

    // if id is an internal id, just return
    if (g_hash_table_contains(self->internal_ids, GUINT_TO_POINTER(id))) {
        return 0;
    }

    dbus_notifications_emit_action_invoked(self->dbus, id, action_key);
    return 0;
}

NotificationsService *notification_service_get_global() { return global; };

int notifications_service_global_init() {
    global = g_object_new(NOTIFICATIONS_SERVICE_TYPE, NULL);
    return 0;
}

NotificationsService *notifications_service_get_global() {
    if (!global) {
        g_debug(
            "notifications_service.c:notifications_service_get_global() "
            "creating global instance");
        global = g_object_new(NOTIFICATIONS_SERVICE_TYPE, NULL);
    }
    return global;
}

GPtrArray *notifications_service_get_notifications(NotificationsService *self) {
    return self->notifications;
}
