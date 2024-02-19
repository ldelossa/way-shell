#include <adwaita.h>

G_BEGIN_DECLS

struct _LogindService;
#define LOGIND_SERVICE_TYPE logind_service_get_type()
G_DECLARE_FINAL_TYPE(LogindService, logind_service, LOGIND, SERVICE, GObject);

G_END_DECLS

int logind_service_global_init(void);

LogindService *logind_service_get_global();

gboolean logind_service_can_reboot(LogindService *self);

void logind_service_reboot(LogindService *self);

gboolean logind_service_can_power_off(LogindService *self);

void logind_service_power_off(LogindService *self);

gboolean logind_service_can_suspend(LogindService *self);

void logind_service_suspend(LogindService *self);

gboolean logind_service_can_hibernate(LogindService *self);

void logind_service_hibernate(LogindService *self);

gboolean logind_service_can_hybrid_sleep(LogindService *self);

void logind_service_hybrid_sleep(LogindService *self);

gboolean logind_service_can_suspendthenhibernate(LogindService *self);

void logind_service_suspendthenhibernate(LogindService *self);

void logind_service_kill_session(LogindService *self);

