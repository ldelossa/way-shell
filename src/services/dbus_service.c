
#include "dbus_service.h"

#include <adwaita.h>

#include "dbus_dbus.h"

enum signals {
    dbus_session_name_added,
    dbus_system_name_added,
    dbus_system_name_lost,
    dbus_session_name_lost,
    signals_n
};

static DBUSService *global;

struct _DBUSService {
    GObject parent_instance;
    GDBusConnection *system;
    GDBusConnection *session;
    DbusDBus *system_proxy;
    DbusDBus *session_proxy;
    GHashTable *system_names;
    GHashTable *session_names;
};
static guint signals[signals_n] = {0};
G_DEFINE_TYPE(DBUSService, dbus_service, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init methods
static void dbus_service_dispose(GObject *obj) {
    G_OBJECT_CLASS(dbus_service_parent_class)->dispose(obj);
}

static void dbus_service_finalize(GObject *obj) {
    G_OBJECT_CLASS(dbus_service_parent_class)->finalize(obj);
}

static void dbus_service_class_init(DBUSServiceClass *cls) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(cls);
    gobject_class->dispose = dbus_service_dispose;
    gobject_class->finalize = dbus_service_finalize;

    signals[dbus_system_name_added] = g_signal_new(
        "dbus-system-name-added", G_TYPE_FROM_CLASS(cls), G_SIGNAL_RUN_FIRST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_HASH_TABLE, G_TYPE_STRING);

    signals[dbus_system_name_lost] = g_signal_new(
        "dbus-system-name-lost", G_TYPE_FROM_CLASS(cls), G_SIGNAL_RUN_FIRST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_HASH_TABLE, G_TYPE_STRING);

    signals[dbus_session_name_added] = g_signal_new(
        "dbus-session-name-added", G_TYPE_FROM_CLASS(cls), G_SIGNAL_RUN_FIRST,
        0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_HASH_TABLE, G_TYPE_STRING);

    signals[dbus_session_name_lost] = g_signal_new(
        "dbus-session-name-lost", G_TYPE_FROM_CLASS(cls), G_SIGNAL_RUN_FIRST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_HASH_TABLE, G_TYPE_STRING);
}

static void on_connection_closed(GDBusConnection *connection,
                                 gboolean remote_peer_vanished, GError *error,
                                 void *user_data) {
    // g_error indicating which dbus connection closed
    if (connection == dbus_service_get_system_bus(user_data))
        g_error("system bus connection closed: %s", error->message);
    else if (connection == dbus_service_get_session_bus(user_data))
        g_error("session bus connection closed: %s", error->message);
}

static void on_system_name_owner_changed(GDBusConnection *connection,
                                         gchar *name, gchar *old_owner,
                                         gchar *new_owner, DBUSService *self) {
    g_debug("dbus_service.c:name owner changed on system bus: %s", name);

    if (new_owner && !old_owner) {
        // add name to set
        g_debug("dbus_service.c:adding name to set: %s", name);
        g_hash_table_add(self->system_names, g_strdup(name));
        g_signal_emit(self, signals[dbus_system_name_added], 0,
                      self->system_names, name);
    } else if (!new_owner && old_owner) {
        // remove name from set
        g_debug("dbus_service.c:removing name from set: %s", name);
        g_hash_table_remove(self->system_names, name);
        g_signal_emit(self, signals[dbus_system_name_lost], 0,
                      self->system_names, name);
    }
}

static void on_session_name_owner_changed(GDBusConnection *connection,
                                          gchar *name, gchar *old_owner,
                                          gchar *new_owner, DBUSService *self) {
    g_debug("dbus_service.c:name owner changed on session bus: %s", name);

    // not sure why, but we get nils here sometime, just ignore them for now...
    if (!new_owner || !old_owner) return;

    if (strlen(old_owner) == 0 && strlen(new_owner) != 0) {
        // add name to set
        g_debug("dbus_service.c:adding name to set: %s", name);
        g_hash_table_add(self->session_names, g_strdup(name));
        g_signal_emit(self, signals[dbus_session_name_added], 0,
                      self->session_names, name);
    } else if (strlen(old_owner) != 0 && strlen(new_owner) == 0) {
        // remove name from set
        g_debug("dbus_service.c:removing name from set: %s", name);
        g_hash_table_remove(self->session_names, name);
        g_signal_emit(self, signals[dbus_session_name_lost], 0,
                      self->session_names, name);
    }
}

static void dbus_service_init(DBUSService *self) {
    GError *error = NULL;

    self->session_names = g_hash_table_new(g_str_hash, g_str_equal);
    self->system_names = g_hash_table_new(g_str_hash, g_str_equal);

    self->system = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (error) {
        g_error("failed to get dbus system bus: %s", error->message);
        g_error_free(error);
    }

    self->session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (error) {
        g_error("failed to get dbus session bus: %s", error->message);
        g_error_free(error);
    }

    self->system_proxy =
        dbus_dbus_proxy_new_sync(self->system, 0, "org.freedesktop.DBus",
                                 "/org/freedesktop/DBus", NULL, &error);
    if (!self->system_proxy)
        g_error("failed to create system proxy: %s", error->message);

    // seed system names
    char **list = NULL;
    error = NULL;
    if (!dbus_dbus_call_list_names_sync(self->system_proxy, &list, NULL,
                                        &error))
        g_error("failed to list system bus names: %s", error->message);

    while (*list) {
        on_system_name_owner_changed(self->system, *list, NULL, (void *)1,
                                     self);
        list++;
    }

    self->session_proxy =
        dbus_dbus_proxy_new_sync(self->session, 0, "org.freedesktop.DBus",
                                 "/org/freedesktop/DBus", NULL, &error);
    if (!self->session_proxy)
        g_error("failed to create session proxy: %s", error->message);

    // seed session names
    list = NULL;
    error = NULL;
    if (!dbus_dbus_call_list_names_sync(self->session_proxy, &list, NULL,
                                        &error))
        g_error("failed to list session bus names: %s", error->message);
    while (*list) {
        on_session_name_owner_changed(self->session, *list, NULL, (void *)1,
                                      self);
        list++;
    }

    // wire into closed signal
    g_signal_connect(self->system, "closed", G_CALLBACK(on_connection_closed),
                     self);
    g_signal_connect(self->session, "closed", G_CALLBACK(on_connection_closed),
                     self);

    // wire into closed signal
    g_signal_connect(self->system_proxy, "name-owner-changed",
                     G_CALLBACK(on_system_name_owner_changed), self);
    g_signal_connect(self->session_proxy, "name-owner-changed",
                     G_CALLBACK(on_session_name_owner_changed), self);
}

// stub out global_init method
int dbus_service_global_init(void) {
    global = g_object_new(DBUS_SERVICE_TYPE, NULL);
    return 0;
}

DBUSService *dbus_service_get_global() { return global; }

// stub out get_system_bus method
GDBusConnection *dbus_service_get_system_bus(DBUSService *self) {
    return self->system;
}

// stub out get_session_bus method
GDBusConnection *dbus_service_get_session_bus(DBUSService *self) {
    return self->session;
}

GHashTable *dbus_service_get_bus_names(DBUSService *self, gboolean system) {
    if (system)
        return self->system_names;
    else
        return self->session_names;
}
