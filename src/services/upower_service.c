#include "upower_service.h"

#include <adwaita.h>
#include <upower.h>

static UPowerService *global = NULL;

struct _UPowerService {
    GObject parent_instance;
    UpClient *client;
    UpDevice *bat;
    UpDevice *ac;
    GPtrArray *devices;
    gboolean on_bat;
    gboolean enabled;
};
G_DEFINE_TYPE(UPowerService, upower_service, G_TYPE_OBJECT);

static void upower_service_dispose(GObject *gobject) {
    UPowerService *self = UPOWER_SERVICE(gobject);

    // Chain-up
    G_OBJECT_CLASS(upower_service_parent_class)->dispose(gobject);
};

static void upower_service_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(upower_service_parent_class)->finalize(gobject);
};

static void upower_service_class_init(UPowerServiceClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = upower_service_dispose;
    object_class->finalize = upower_service_finalize;
};

static void upower_service_set_battery_device(UPowerService *self,
                                              UpDevice *device) {
    GValue kind = G_VALUE_INIT;
    g_object_get_property(G_OBJECT(device), "kind", &kind);
    if (g_value_get_uint(&kind) == UP_DEVICE_KIND_BATTERY) {
        g_debug(
            "upower_service.c:upower_service_init_is_battery() discovered "
            "battery\n%s",
            up_device_to_text(device));
        self->bat = device;
    }
};

static void upower_service_set_ac_device(UPowerService *self,
                                         UpDevice *device) {
    GValue kind = G_VALUE_INIT;
    g_object_get_property(G_OBJECT(device), "kind", &kind);
    if (g_value_get_uint(&kind) == UP_DEVICE_KIND_LINE_POWER) {
        // check if native-path is equal to "AC"
        GValue native_path = G_VALUE_INIT;
        g_object_get_property(G_OBJECT(device), "native-path", &native_path);
        if (g_strcmp0(g_value_get_string(&native_path), "AC") != 0) {
            return;
        }

        g_debug(
            "upower_service.c:upower_service_init_is_battery() discovered "
            "AC\n%s",
            up_device_to_text(device));
        self->ac = device;
    }
};

static void upower_service_init(UPowerService *self) {
    self->client = up_client_new();
    self->enabled = TRUE;
    if (self->client == NULL) {
        self->enabled = FALSE;
    }
    // get devices
    self->devices = up_client_get_devices2(self->client);
    // find battery device if it exist.
    for (guint i = 0; i < self->devices->len; i++) {
        if (!self->bat)
            upower_service_set_battery_device(self, self->devices->pdata[i]);
        if (!self->ac)
            upower_service_set_ac_device(self, self->devices->pdata[i]);
    }
};

int upower_service_global_init(void) {
    g_debug(
        "upower.c:upower_service_global_init() initializing upower service.");
    global = g_object_new(UPOWER_SERVICE_TYPE, NULL);
    if (!global->enabled) {
        g_clear_object(&global);
        return -1;
    };
    return 0;
};

UpClient *upower_service_get_up_client(UPowerService *self) {
    return self->client;
};

// Get the global upower service
// Will return NULL if `clock_service_global_init` has not been called.
UPowerService *upower_service_get_global() { return global; };

// Returns the primary power source for the system.
// If a battery is present this will be the first battery on the system.
// If no battery is present this will be the AC line power device.
UpDevice *upower_service_get_primary_device(UPowerService *self) {
    if (!self) {
        return NULL;
    }
    if (self->bat) {
        return self->bat;
    }
    if (self->ac) {
        return self->ac;
    }
    return NULL;
}

// Returns whether the primary device is a battery or an AC power supply.
gboolean upower_service_primary_is_bat(UPowerService *self) {
    if (self->bat) {
        return TRUE;
    }
    return FALSE;
}

char *upower_device_map_icon_name(UpDevice *device) {
    double percent = 0;
    gboolean rechargable = 0;
    guint state = 0;
    gchar *icon = NULL;
    gboolean charging = false;

    g_debug("upower_service.c:upower_device_map_icon_name() called.");

    g_object_get(device, "percentage", &percent, NULL);
    g_object_get(device, "is-rechargeable", &rechargable, NULL);
    g_object_get(device, "state", &state, NULL);
    g_object_get(device, "icon-name", &icon, NULL);

    g_debug(
        "upower_service.c:upower_device_map_icon_name() icon: %s state: %d "
        "rechargable: %d percent: %f",
        icon, state, rechargable, percent);

    if (!rechargable) {
        return "ac-adapter-symbolic";
    }

    charging = (state == UP_DEVICE_STATE_CHARGING ||
                state == UP_DEVICE_STATE_FULLY_CHARGED ||
                state == UP_DEVICE_STATE_PENDING_CHARGE);

    if (percent <= 10) {
        if (charging) return "battery-caution-charging-symbolic";
        return "battery-level-0-symbolic";
    }
    if (percent > 10 && percent < 20) {
        if (charging) return "battery-level-10-charging-symbolic";
        return "battery-level-10-symbolic";
    }
    if (percent >= 20 && percent < 30) {
        if (charging) return "battery-level-20-charging-symbolic";
        return "battery-level-20-symbolic";
    }
    if (percent >= 30 && percent < 40) {
        if (charging) return "battery-level-30-charging-symbolic";
        return "battery-level-30-symbolic";
    }
    if (percent >= 40 && percent < 50) {
        if (charging) return "battery-level-40-charging-symbolic";
        return "battery-level-40-symbolic";
    }
    if (percent >= 50 && percent < 60) {
        if (charging) return "battery-level-50-charging-symbolic";
        return "battery-level-50-symbolic";
    }
    if (percent >= 60 && percent < 70) {
        if (charging) return "battery-level-60-charging-symbolic";
        return "battery-level-60-symbolic";
    }
    if (percent >= 70 && percent < 80) {
        if (charging) return "battery-level-70-charging-symbolic";
        return "battery-level-70-symbolic";
    }
    if (percent >= 80 && percent < 90) {
        if (charging) return "battery-level-80-charging-symbolic";
        return "battery-level-80-symbolic";
    }
    if (percent > 90) {
        if (charging) {
            if (state == UP_DEVICE_STATE_FULLY_CHARGED)
                return "battery-full-charging-symbolic";
            else
                return "battery-level-90-charging-symbolic";
        }
        return "battery-level-90-symbolic";
    }
    return "battery-missing-symbolic";
}
