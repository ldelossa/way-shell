#pragma once

#include <NetworkManager.h>
#include <adwaita.h>

#include "nm-core-types.h"

G_BEGIN_DECLS

// Simple clock service which emits a 'tick' signal on every minute.
// The clock is synchronized to the next minute boundary following its
// construction.
//
// `tick` event provides a GDateTime as its first argument.
struct _NetworkManagerService;
#define NETWORK_MANAGER_SERVICE_TYPE network_manager_service_get_type()
G_DECLARE_FINAL_TYPE(NetworkManagerService, network_manager_service,
                     NETWORK_MANAGER, SERVICE, GObject);

G_END_DECLS

int network_manager_service_global_init(void);

// Get the global clock service
// Will return NULL if `network_manager_service_global_init` has not been
// called.
NetworkManagerService *network_manager_service_get_global();

const GPtrArray *network_manager_service_get_devices(
    NetworkManagerService *self);

gboolean network_manager_service_wifi_available(NetworkManagerService *self);

gboolean network_manager_service_ethernet_available(
    NetworkManagerService *self);

NMState network_manager_service_get_state(NetworkManagerService *self);

// A simplification of Device state which returns
// NM_DEVICE_STATE_UNKNOWN if there are no wifi devices on the system
// NM_DEVICE_STATE_DISCONNECTED if all wifi devices are disconnected
// NM_DEVICE_STATE_PREPARE if at least one wifi device is activating
// NM_DEVICE_STATE_ACTIVATED if at least one wifi device is activate
NMDeviceState network_manager_service_wifi_state(NetworkManagerService *self);

NMDevice *network_manager_service_get_primary_device(
    NetworkManagerService *self);

char *network_manager_service_ap_strength_to_icon_name(guchar strength);

char *network_manager_service_ap_to_name(NMAccessPoint *ap);

void network_manager_service_ap_join(NetworkManagerService *self,
                                     NMDeviceWifi *dev, NMAccessPoint *ap,
                                     const char *password);

void network_manager_service_ap_disconnect(NetworkManagerService *self,
                                           NMDeviceWifi *dev);

void network_manager_service_wireless_enable(NetworkManagerService *self,
                                             gboolean enabled);

void network_manager_service_networking_enable(NetworkManagerService *self,
                                               gboolean enabled);

gboolean network_manager_has_vpn(NetworkManagerService *self);

GHashTable *network_manager_get_vpn_connections(NetworkManagerService *self);

NMVpnConnection *network_manager_get_active_vpn_connection(
    NetworkManagerService *self);

void network_manager_activate_vpn(NetworkManagerService *self, const gchar *id,
                                  gboolean activate);

gboolean network_manager_service_get_networking_enabled(
    NetworkManagerService *self);
