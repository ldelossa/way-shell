#include "power_profiles_service.h"

#include <adwaita.h>

#include "power_profiles_dbus.h"
#include "../../services/dbus_service.h"

static PowerProfilesService *global = NULL;

enum signals { profiles_changed, active_profile_changed, signals_n };

struct _PowerProfilesService {
    GObject parent_instance;
    DbusPowerProfiles *dbus;
    GDBusConnection *conn;
    const char *active_profile;
    GArray *profiles;
    gboolean enabled;
};
static guint signals[signals_n] = {0};
G_DEFINE_TYPE(PowerProfilesService, power_profiles_service, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init methods
static void power_profiles_service_dispose(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(power_profiles_service_parent_class)->dispose(gobject);
};

static void power_profiles_service_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(power_profiles_service_parent_class)->finalize(gobject);
};

static void power_profiles_service_class_init(
    PowerProfilesServiceClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = power_profiles_service_dispose;
    object_class->finalize = power_profiles_service_finalize;

    // signals
    signals[active_profile_changed] = g_signal_new(
        "active-profile-changed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST,
        0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);

    signals[profiles_changed] = g_signal_new(
        "profiles-changed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_ARRAY);
};

static void power_profiles_service_dbus_connect(PowerProfilesService *self) {
    GError *error = NULL;

	DBUSService *dbus = dbus_service_get_global();
	self->conn = dbus_service_get_system_bus(dbus);

    self->dbus = dbus_power_profiles_proxy_new_sync(
        self->conn, G_DBUS_PROXY_FLAGS_NONE, "net.hadess.PowerProfiles",
        "/net/hadess/PowerProfiles", NULL, &error);
    if (!self->dbus)
        g_error(
            "power_profiles_service.c:power_profiles_service_dbus_connect(): "
            "error: "
            "export dbus interface: %s",
            error->message);
}

static void on_power_profiles_service_active_profile_change(
    DbusPowerProfiles *dbus, GParamSpec *pspec, PowerProfilesService *self) {
    g_debug(
        "power_profiles_service.c:on_power_profiles_service_active_profile_"
        "change() called");

    char *active_profile =
        g_strdup(dbus_power_profiles_get_active_profile(dbus));

    // there is a chance this can be NULL if the service was restarted.
    if (!active_profile) return;

    self->active_profile = active_profile;

    // emit signal
    g_signal_emit(self, signals[active_profile_changed], 0,
                  g_strdup(self->active_profile));
}

static void on_power_profiles_service_profiles_change(
    DbusPowerProfiles *dbus, GParamSpec *pspec, PowerProfilesService *self) {
    g_debug(
        "power_profiles_service.c:on_power_profiles_service_profiles_change() "
        "called");

    for (guint i = 0; i < self->profiles->len; i++) {
        g_free(g_array_index(self->profiles, gchar *, i));
    }
    g_array_set_size(self->profiles, 0);

    // extract profile strings from from 'aa{sv}'
    GVariant *profiles = dbus_power_profiles_get_profiles(dbus);
	if (!profiles)
		return;

    if (!g_variant_is_of_type(profiles, G_VARIANT_TYPE("aa{sv}"))) {
        g_error("Variant is not of type 'aa{sv}'");
    }

    GVariantIter outer_iter;
    GVariant *dict = NULL;
    gchar *profile;

    g_variant_iter_init(&outer_iter, profiles);
    while (g_variant_iter_next(&outer_iter, "@a{sv}", &dict)) {
        g_variant_lookup(dict, "Profile", "s", &profile);
        char *duplicated_profile = g_strdup(profile);
        g_array_append_val(self->profiles, duplicated_profile);
        g_variant_unref(dict);
    }

    g_variant_unref(profiles);

    // emit signal
    g_signal_emit(self, signals[profiles_changed], 0, self->profiles);
}

static void power_profiles_service_init(PowerProfilesService *self) {
    g_debug("power_profiles_service.c:power_profiles_service_init() called");
    power_profiles_service_dbus_connect(self);

    self->profiles = g_array_new(FALSE, FALSE, sizeof(gchar *));

    // profiles
    on_power_profiles_service_profiles_change(self->dbus, NULL, self);

    // get active profile
    on_power_profiles_service_active_profile_change(self->dbus, NULL, self);

    // subscribe to self->debug notify 'active_profile' property
    g_signal_connect(
        self->dbus, "notify::active-profile",
        G_CALLBACK(on_power_profiles_service_active_profile_change), self);

    // subscribe to profiles
    g_signal_connect(
        self->dbus, "notify::profiles",
        G_CALLBACK(on_power_profiles_service_active_profile_change), self);
};

int power_profiles_service_global_init(void) {
    g_debug(
        "power_profiles_service.c:power_profiles_service_global_init() "
        "called");
    if (global) return 0;

    global = g_object_new(POWER_PROFILES_SERVICE_TYPE, NULL);
    return 0;
}

PowerProfilesService *power_profiles_service_get_global() {
    g_debug(
        "power_profiles_service.c:power_profiles_service_get_global() called");
    return global;
}

GArray *power_profiles_service_get_profiles(PowerProfilesService *self) {
    g_debug(
        "power_profiles_service.c:power_profiles_service_get_profiles() "
        "called");

    return self->profiles;
}

void power_profiles_service_set_profile(PowerProfilesService *self,
                                        gchar *profile) {
    g_debug(
        "power_profiles_service.c:power_profiles_service_set_profile() "
        "called");
    dbus_power_profiles_set_active_profile(self->dbus, profile);
}

const gchar *power_profiles_service_get_active_profile(
    PowerProfilesService *self) {
    g_debug(
        "power_profiles_service.c:power_profiles_service_get_active_profile() "
        "called");
    return g_strdup(self->active_profile);
}

const char *power_profiles_service_profile_to_icon(const char *profile) {
	if (!profile)
        return "power-profile-balanced-symbolic";

    if (strcmp(profile, "performance") == 0) {
        return "power-profile-performance-symbolic";
    } else if (strcmp(profile, "balanced") == 0) {
        return "power-profile-balanced-symbolic";
    } else if (strcmp(profile, "power-saver") == 0) {
        return "power-profile-power-saver-symbolic";
    } else {
        return "power-profile-balanced-symbolic";
    }
}
