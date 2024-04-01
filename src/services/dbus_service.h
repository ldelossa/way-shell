#include <adwaita.h>

typedef struct _DBUS {
    GDBusConnection *system;
    GDBusConnection *session;
} DBUS;

G_BEGIN_DECLS

struct _DBUSService;
#define DBUS_SERVICE_TYPE dbus_service_get_type()
G_DECLARE_FINAL_TYPE(DBUSService, dbus_service, DBUS, SERVICE, GObject);

G_END_DECLS

int dbus_service_global_init(void);

DBUSService *dbus_service_get_global();

GDBusConnection *dbus_service_get_system_bus(DBUSService *self);

GDBusConnection *dbus_service_get_session_bus(DBUSService *self);
