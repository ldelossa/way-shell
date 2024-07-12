#include "network_manager_service.h"

#include <NetworkManager.h>
#include <adwaita.h>

#include "glib-object.h"
#include "glib.h"
#include "nm-core-types.h"
#include "nm-dbus-interface.h"

static NetworkManagerService *global = NULL;

enum signals {
    changed,
    networking_enabled,
    vpn_added,
    vpn_removed,
    vpn_activated,
    vpn_deactivated,
    signals_n
};

struct _NetworkManagerService {
    GObject parent_instance;
    NMClient *client;
    NMDevice *primary_dev;
    NMState last_state;
    gboolean has_wifi;
    gboolean has_ethernet;
    gboolean has_vpn;
    GHashTable *vpn_conns;
    NMVpnConnection *active_vpn;
    struct {
        NMDeviceWifi *dev;
        NMAccessPoint *ap;
    } wireless_cache;
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
    signals[networking_enabled] = g_signal_new(
        "networking-enabled-changed", G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    signals[vpn_added] = g_signal_new(
        "vpn-added", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        NULL, G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_INT);

    signals[vpn_removed] = g_signal_new(
        "vpn-removed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0, NULL,
        NULL, NULL, G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_INT);

    signals[vpn_activated] = g_signal_new(
        "vpn-activated", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0, NULL,
        NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);

    signals[vpn_deactivated] = g_signal_new(
        "vpn-deactivated", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0, NULL,
        NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);
};

static void on_changed(NMClient *client, GParamSpec *spec,
                       NetworkManagerService *self) {
    g_debug("network_manager_service.c:on_change() called");

    NMState state = 0;
    NMDevice *dev = NULL;
    NMActiveConnection *conn = NULL;

    self->has_ethernet = false;
    self->has_wifi = false;
    self->has_vpn = false;

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

static void on_networking_enabled_changed(NMClient *client, GParamSpec *_,
                                          NetworkManagerService *self) {
    gboolean enabled = nm_client_networking_get_enabled(client);

    // emit signals
    if (enabled) {
        g_signal_emit(self, signals[networking_enabled], 0, true);
    } else {
        g_signal_emit(self, signals[networking_enabled], 0, false);
    }
}

static void on_vpn_connection_added(NMClient *client, NMConnection *conn,
                                    NetworkManagerService *self) {
    // We only care about tracking VPN connections.
    const gchar *type = nm_connection_get_connection_type(conn);
    const gchar *id = nm_connection_get_id(conn);

    if (g_strcmp0(type, "vpn") != 0) return;

    g_hash_table_insert(self->vpn_conns, strdup(id), conn);
    int len = g_hash_table_size(self->vpn_conns);

    self->has_vpn = true;

    g_signal_emit(self, signals[vpn_added], 0, conn, len);
}

static void on_vpn_connection_removed(NMClient *client, NMConnection *conn,
                                      NetworkManagerService *self) {
    // We only care about tracking VPN connections.
    const gchar *type = nm_connection_get_connection_type(conn);
    const gchar *id = nm_connection_get_id(conn);

    if (g_strcmp0(type, "vpn") != 0) return;

    g_hash_table_remove(self->vpn_conns, id);
    int len = g_hash_table_size(self->vpn_conns);

    if (len == 0) self->has_vpn = false;

    g_signal_emit(self, signals[vpn_removed], 0, conn, len);
}

static void on_active_vpn_connection_added(NMClient *client,
                                           NMActiveConnection *conn,
                                           NetworkManagerService *self) {
    // We only care about tracking VPN connections.
    if (!nm_active_connection_get_vpn(conn)) return;

    self->active_vpn = (NMVpnConnection *)conn;

    g_signal_emit(self, signals[vpn_activated], 0, conn);
}

static void on_active_vpn_connection_removed(NMClient *client,
                                             NMActiveConnection *conn,
                                             NetworkManagerService *self) {
    // We only care about tracking VPN connections.
    if (!nm_active_connection_get_vpn(conn)) return;

    if (!self->active_vpn) return;

    const gchar *id = nm_active_connection_get_id(conn);
    const gchar *cur_id =
        nm_active_connection_get_id((NMActiveConnection *)self->active_vpn);

    if (g_strcmp0(id, cur_id) != 0) return;

    g_signal_emit(self, signals[vpn_deactivated], 0, conn);
}

static void network_manager_service_init(NetworkManagerService *self) {
    GError *error;

    self->vpn_conns = g_hash_table_new(g_str_hash, g_str_equal);

    self->client = nm_client_new(NULL, &error);
    if (!self->client) {
        g_error("Failed to create NetworkManager client: %s", error->message);
        g_error_free(error);
        return;
    }

    on_changed(self->client, NULL, self);

    // populate existing VPN networks
    const GPtrArray *connections = nm_client_get_connections(self->client);
    for (int i = 0; i < connections->len; i++) {
        NMConnection *conn = connections->pdata[i];
        const gchar *type = nm_connection_get_connection_type(conn);
        if (g_strcmp0(type, "vpn") == 0) {
            g_object_ref(conn);
            g_hash_table_insert(self->vpn_conns,
                                g_strdup(nm_connection_get_id(conn)), conn);
            self->has_vpn = true;
        }
    }

    g_signal_connect(self->client, "notify", G_CALLBACK(on_changed), self);

    g_signal_connect(self->client, "notify::networking-enabled",
                     G_CALLBACK(on_networking_enabled_changed), self);

    g_signal_connect(self->client, "connection-added",
                     G_CALLBACK(on_vpn_connection_added), self);

    g_signal_connect(self->client, "connection-removed",
                     G_CALLBACK(on_vpn_connection_removed), self);

    g_signal_connect(self->client, "active-connection-added",
                     G_CALLBACK(on_active_vpn_connection_added), self);

    g_signal_connect(self->client, "active-connection-removed",
                     G_CALLBACK(on_active_vpn_connection_removed), self);
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

char *network_manager_service_ap_strength_to_icon_name(guchar strength) {
    if (strength < 25) {
        return "network-wireless-signal-weak-symbolic";
    } else if (strength < 50) {
        return "network-wireless-signal-ok-symbolic";
    } else if (strength < 75) {
        return "network-wireless-signal-good-symbolic";
    } else {
        return "network-wireless-signal-excellent-symbolic";
    }
};

char *network_manager_service_ap_to_name(NMAccessPoint *ap) {
    GBytes *bytes = nm_access_point_get_ssid(ap);

    if (!bytes) return "";

    if (nm_utils_is_empty_ssid(g_bytes_get_data(bytes, NULL),
                               g_bytes_get_size(bytes)))
        return "";

    char *ssid = nm_utils_ssid_to_utf8(g_bytes_get_data(bytes, NULL),
                                       g_bytes_get_size(bytes));
    return ssid;
}

static void on_ap_join(GObject *source_object, GAsyncResult *res,
                       gpointer data) {
    g_debug("network_manager_service.c:on_ap_join() called");

    // parse out GAsyncResult and print error
    GError *error = NULL;
    NMClient *client = NM_CLIENT(source_object);
    NetworkManagerService *self = NETWORK_MANAGER_SERVICE(data);
    NMActiveConnection *conn =
        nm_client_add_and_activate_connection_finish(client, res, &error);

    if (error) {
        g_debug(
            "network_manager_service.c:on_ap_join() failed to join access "
            "point: %s",
            error->message);
        g_error_free(error);
        return;
    }

    g_debug(
        "network_manager_service.c:on_ap_join() successfully joined access "
        "point");
}

static void on_remote_conn_sync(GObject *source_object, GAsyncResult *res,
                                gpointer data) {
    // parse out GAsyncResult and print error
    GError *error = NULL;
    NetworkManagerService *self = NETWORK_MANAGER_SERVICE(data);
    NMRemoteConnection *conn = NM_REMOTE_CONNECTION(source_object);
    GCancellable *cancel = g_cancellable_new();

    nm_remote_connection_commit_changes_finish(conn, res, &error);

    if (error) {
        g_debug(
            "network_manager_service.c:on_ap_join() failed to join access "
            "point: %s",
            error->message);
        g_error_free(error);
        return;
    }

    g_debug(
        "network_manager_service.c:on_ap_join() successfully synced remote "
        "connection ");

    nm_client_activate_connection_async(self->client, NM_CONNECTION(conn),
                                        NM_DEVICE(self->wireless_cache.dev),
                                        NULL, cancel, on_ap_join, self);

    // clear cache
    self->wireless_cache.dev = NULL;
    self->wireless_cache.ap = NULL;
}

void network_manager_service_ap_join(NetworkManagerService *self,
                                     NMDeviceWifi *dev, NMAccessPoint *ap,
                                     const char *password) {
    g_debug(
        "network_manager_service.c:network_manager_service_wifi_join() called");

    // debug password
    g_debug(
        "network_manager_service.c:network_manager_service_wifi_join() "
        "password: %s",
        password);

    NMClient *client = self->client;
    GBytes *ap_ssid = nm_access_point_get_ssid(ap);
    NMConnection *found_conn = NULL;
    gboolean new = false;
    GCancellable *cancel = g_cancellable_new();

    if (!dev || !ap || !self) {
        g_debug(
            "network_manager_service.c:network_manager_service_wifi_join() "
            "missing required arguments");
        return;
    }

    const GPtrArray *connections = nm_client_get_connections(self->client);

    for (int i = 0; i < connections->len; i++) {
        NMConnection *conn = connections->pdata[i];
        NMSettingWireless *wireless = nm_connection_get_setting_wireless(conn);
        if (!wireless) continue;

        GBytes *conn_ssid = nm_setting_wireless_get_ssid(wireless);
        if (g_bytes_equal(ap_ssid, conn_ssid)) {
            g_debug(
                "network_manager_service.c:network_manager_service_wifi_join() "
                "found matching connection");
            found_conn = conn;
            break;
        }
    }

    // didn't find one...
    if (!found_conn) {
        new = true;
        char *ssid_name = network_manager_service_ap_to_name(ap);
        found_conn = nm_simple_connection_new();

        NMSettingConnection *conn_settings =
            NM_SETTING_CONNECTION(nm_setting_connection_new());
        g_object_set(conn_settings, NM_SETTING_CONNECTION_ID, ssid_name,
                     NM_SETTING_CONNECTION_AUTOCONNECT, true, NULL);
        nm_connection_add_setting(found_conn, NM_SETTING(conn_settings));

        NMSettingWireless *wireless_settings =
            NM_SETTING_WIRELESS(nm_setting_wireless_new());
        g_object_set(wireless_settings, NM_SETTING_WIRELESS_SSID, ap_ssid,
                     NULL);
        nm_connection_add_setting(found_conn, NM_SETTING(wireless_settings));

        g_debug(
            "network_manager_service.c:network_manager_service_wifi_join() "
            "created new connection [%s] and connecting...",
            ssid_name);
    }

    // update the password even if we found a conn, the UI does not currently
    // check for cached passwords and requires the user to enter a password.
    //
    // this is kinda useful because if the password has changed, they can just
    // re-enter it withou any special conditions in the UI code.
    // this may change tho if it becomes too inconenvient to put in a password
    // when switching between known networks...
    if (password) {
        g_debug(
            "network_manager_service.c:network_manager_service_wifi_join() "
            "setting password: %s",
            password);
        NMSettingWirelessSecurity *sec_settings =
            NM_SETTING_WIRELESS_SECURITY(nm_setting_wireless_security_new());
        g_object_set(sec_settings, NM_SETTING_WIRELESS_SECURITY_PSK, password,
                     NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-psk", NULL);
        nm_connection_add_setting(found_conn, NM_SETTING(sec_settings));
    }

    if (new) {
        nm_client_add_and_activate_connection_async(
            client, found_conn, NM_DEVICE(dev), NULL, cancel, on_ap_join, self);
    } else {
        // stash Wifi device and AP in cache for next callback
        self->wireless_cache.dev = dev;
        self->wireless_cache.ap = ap;

        g_debug(
            "network_manager_service.c:network_manager_service_wifi_join() "
            "found existing connection, syncing changes...");

        // if connection existed, sync the password change back up to NM and
        // save it to disk.
        nm_remote_connection_commit_changes_async(
            NM_REMOTE_CONNECTION(found_conn), true, cancel, on_remote_conn_sync,
            self);
    }
}

static void on_wifi_disconnect(GObject *source_object, GAsyncResult *res,
                               gpointer data) {
    GError *error = NULL;
    NMClient *client = NM_CLIENT(source_object);
    nm_client_deactivate_connection_finish(client, res, &error);

    if (error) {
        g_debug(
            "network_manager_service.c:on_ap_join() failed to disconnect wifi: "
            "%s",
            error->message);
        g_error_free(error);
        return;
    }
}

void network_manager_service_ap_disconnect(NetworkManagerService *self,
                                           NMDeviceWifi *dev) {
    NMActiveConnection *active_con =
        nm_device_get_active_connection(NM_DEVICE(dev));

    if (!active_con) return;

    nm_client_deactivate_connection_async(self->client, active_con, NULL,
                                          on_wifi_disconnect, self);
}

void network_manager_service_wireless_enable(NetworkManagerService *self,
                                             gboolean enabled) {
    g_object_set(G_OBJECT(self->client), "wireless-enabled", enabled, NULL);
}

void network_manager_service_networking_enable(NetworkManagerService *self,
                                               gboolean enabled) {
    g_object_set(G_OBJECT(self->client), "networking-enabled", enabled, NULL);
}

gboolean network_manager_service_get_networking_enabled(
    NetworkManagerService *self) {
    gboolean enabled = false;
    enabled = nm_client_networking_get_enabled(self->client);
    return enabled;
};

gboolean network_manager_has_vpn(NetworkManagerService *self) {
    return self->has_vpn;
}

GHashTable *network_manager_get_vpn_connections(NetworkManagerService *self) {
    return self->vpn_conns;
}

static void on_vpn_activated(GObject *source_object, GAsyncResult *res,
                             gpointer data) {
    GError *error = NULL;
    NMClient *client = NM_CLIENT(source_object);
    nm_client_activate_connection_finish(client, res, &error);

    if (error) {
        g_debug(
            "network_manager_service.c:on_vpn_activated() failed to activate "
            "vpn: %s",
            error->message);
        g_error_free(error);
        return;
    }
}

static void on_vpn_deactivated(GObject *source_object, GAsyncResult *res,
                               gpointer data) {
    GError *error = NULL;
    NMClient *client = NM_CLIENT(source_object);
    nm_client_deactivate_connection_finish(client, res, &error);

    if (error) {
        g_debug(
            "network_manager_service.c:on_vpn_activated() failed to deactivate "
            "vpn: %s",
            error->message);
        g_error_free(error);
        return;
    }
}

void network_manager_activate_vpn(NetworkManagerService *self, const gchar *id,
                                  gboolean activate) {
    if (activate) {
        NMConnection *conn = g_hash_table_lookup(self->vpn_conns, id);
        if (!conn) return;

        nm_client_activate_connection_async(self->client, conn,
                                            self->primary_dev, NULL, NULL,
                                            on_vpn_activated, self);
    } else {
        NMActiveConnection *conn =
            nm_client_get_primary_connection(self->client);
        if (!conn) return;

        nm_client_deactivate_connection_async(self->client, conn, NULL,
                                              on_vpn_deactivated, self);
    }
}

NMVpnConnection *network_manager_get_active_vpn_connection(
    NetworkManagerService *self) {
    NMActiveConnection *conn = nm_client_get_primary_connection(self->client);

    if (nm_active_connection_get_vpn(conn)) return (NMVpnConnection *)conn;

    return NULL;
}
