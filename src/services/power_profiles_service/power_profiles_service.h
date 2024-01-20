#include <adwaita.h>

G_BEGIN_DECLS

struct _PowerProfilesService;
#define POWER_PROFILES_SERVICE_TYPE power_profiles_service_get_type()
G_DECLARE_FINAL_TYPE(PowerProfilesService, power_profiles_service,
                     POWER_PROFILES, SERVICE, GObject);

G_END_DECLS

int power_profiles_service_global_init(void);

// Get the global clock service
// Will return NULL if `clock_service_global_init` has not been called.
PowerProfilesService *power_profiles_service_get_global();

GArray *power_profiles_service_get_profiles(PowerProfilesService *self);

void power_profiles_service_set_profile(PowerProfilesService *self,
                                        gchar *profile);

const gchar *power_profiles_service_get_active_profile(PowerProfilesService *self);

const char *power_profiles_service_profile_to_icon(const char *profile);
