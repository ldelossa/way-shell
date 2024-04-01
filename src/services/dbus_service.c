
#include "dbus_service.h"

#include <adwaita.h>

enum signals { signals_n };

static DBUSService *global;

struct _DBUSService {
    GObject parent_instance;
    GDBusConnection *system;
    GDBusConnection *session;
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

static void dbus_service_init(DBUSService *self) {
    GError *error = NULL;

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

    // wire into closed signal
    g_signal_connect(self->system, "closed", G_CALLBACK(on_connection_closed),
                     self);
    g_signal_connect(self->session, "closed", G_CALLBACK(on_connection_closed),
                     self);
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
