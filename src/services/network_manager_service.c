#include "network_manager_service.h"

#include <NetworkManager.h>
#include <adwaita.h>

static NetworkManagerService *global = NULL;

enum signals { changed, signals_n };

struct _NetworkManagerService {
    GObject parent_instance;
    NMClient *client;
    NMDevice *primary_dev;
    NMState last_state;
    gboolean has_wifi;
    gboolean has_ethernet;
};
static guint signals[signals_n] = {0};
G_DEFINE_TYPE(NetworkManagerService, network_manager_service, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init methods
static void network_manager_service_dispose(GObject *gobject) {
    NetworkManagerService *self = NETWORK_MANAGER_SERVICE(gobject);

    // Chain-up
    G_OBJECT_CLASS(network_manager_service_parent_class)->dispose(gobject);
};

static void network_manager_service_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(network_manager_service_parent_class)->finalize(gobject);
};

static void network_manager_service_class_init(
    NetworkManagerServiceClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = network_manager_service_dispose;
    object_class->finalize = network_manager_service_finalize;

    signals[changed] =
        g_signal_new("changed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
                     NULL, NULL, NULL, G_TYPE_NONE, 0);
};

static void on_changed(NMClient *client, GParamSpec *spec,
                       NetworkManagerService *self) {
    g_debug("network_manager_service.c:on_change() called");

    NMState state = 0;
    NMDevice *dev = NULL;
    NMActiveConnection *conn = NULL;

    self->has_ethernet = false;
    self->has_wifi = false;

    const GPtrArray *devices = nm_client_get_devices(client);
    for (int i = 0; i < devices->len; i++) {
        NMDeviceType type = nm_device_get_device_type(devices->pdata[i]);
        if (type == NM_DEVICE_TYPE_ETHERNET) {
            self->has_ethernet = true;
        }
        if (type == NM_DEVICE_TYPE_WIFI) {
            self->has_wifi = true;
        }
    }

    state = nm_client_get_state(client);
    self->last_state = state;
    switch (state) {
        case NM_STATE_UNKNOWN:
        case NM_STATE_ASLEEP:
        case NM_STATE_DISCONNECTED:
        case NM_STATE_DISCONNECTING:
        case NM_STATE_CONNECTED_LOCAL:
        case NM_STATE_CONNECTED_SITE:
            dev = NULL;
            break;
        case NM_STATE_CONNECTING:
            conn = nm_client_get_activating_connection(client);
            if (!conn) return;
            dev = g_ptr_array_index(nm_active_connection_get_devices(conn), 0);
            break;
        case NM_STATE_CONNECTED_GLOBAL:
            conn = nm_client_get_primary_connection(client);
            if (!conn) return;
            dev = g_ptr_array_index(nm_active_connection_get_devices(conn), 0);
            break;
        default:
            break;
    }

    if (self->primary_dev) g_object_unref(self->primary_dev);
    self->primary_dev = dev;
    if (self->primary_dev) g_object_ref(self->primary_dev);

    g_signal_emit(self, signals[changed], 0);
};

static void network_manager_service_init(NetworkManagerService *self) {
    GError *error;

    self->client = nm_client_new(NULL, &error);
    if (!self->client) {
        g_error("Failed to create NetworkManager client: %s", error->message);
        g_error_free(error);
        return;
    }

    on_changed(self->client, NULL, self);

    g_signal_connect(self->client, "notify", G_CALLBACK(on_changed), self);
};

const GPtrArray *network_manager_service_get_devices(
    NetworkManagerService *self) {
    return nm_client_get_devices(self->client);
};

NMDevice *network_manager_service_get_primary_device(
    NetworkManagerService *self) {
    return self->primary_dev;
};

gboolean network_manager_service_wifi_available(NetworkManagerService *self) {
    return self->has_wifi;
};

gboolean network_manager_service_ethernet_available(
    NetworkManagerService *self) {
    return self->has_ethernet;
};

NMState network_manager_service_get_state(NetworkManagerService *self) {
    return self->last_state;
};

// A simplification of Device state which returns
// NM_DEVICE_STATE_UNKNOWN if there are no wifi devices on the system
// NM_DEVICE_STATE_DISCONNECTED if all wifi devices are disconnected
// NM_DEVICE_STATE_PREPARE if at least one wifi device is in the process of
// connecting to a network.
// NM_DEVICE_STATE_ACTIVATED if at least one wifi device is activated
NMDeviceState network_manager_service_wifi_state(NetworkManagerService *self) {
    if (!self->has_wifi) return NM_DEVICE_STATE_UNKNOWN;

    NMDeviceState state = 0;
    int activating = 0;
    int activated = 0;

    if (!nm_client_wireless_get_enabled(self->client))
        return NM_DEVICE_STATE_DISCONNECTED;

    const GPtrArray *devices = nm_client_get_devices(self->client);
    for (int i = 0; i < devices->len; i++) {
        NMDevice *dev = devices->pdata[i];

        if (!(nm_device_get_device_type(dev) == NM_DEVICE_TYPE_WIFI)) continue;

        state = nm_device_get_state(dev);
        switch (state) {
            case NM_DEVICE_STATE_UNKNOWN:
            case NM_DEVICE_STATE_UNMANAGED:
            case NM_DEVICE_STATE_UNAVAILABLE:
            case NM_DEVICE_STATE_DISCONNECTED:
            case NM_DEVICE_STATE_FAILED:
            case NM_DEVICE_STATE_DEACTIVATING:
                continue;
            case NM_DEVICE_STATE_PREPARE:
            case NM_DEVICE_STATE_CONFIG:
            case NM_DEVICE_STATE_NEED_AUTH:
            case NM_DEVICE_STATE_IP_CONFIG:
            case NM_DEVICE_STATE_IP_CHECK:
            case NM_DEVICE_STATE_SECONDARIES:
                activating++;
                break;
            case NM_DEVICE_STATE_ACTIVATED:
                activated++;
                break;
        }
    }

    if (activated > 0) return NM_DEVICE_STATE_ACTIVATED;
    if (activating > 0) return NM_DEVICE_STATE_PREPARE;

    return NM_DEVICE_STATE_DISCONNECTED;
};

int network_manager_service_global_init(void) {
    g_debug(
        "network_manager_service.c:network_manager_service_global_init() "
        "initializing global network manager service");
    global = g_object_new(NETWORK_MANAGER_SERVICE_TYPE, NULL);
    return 0;
};

// Get the global clock service
// Will return NULL if `network_manager_service_global_init` has not been
// called.
NetworkManagerService *network_manager_service_get_global() { return global; };