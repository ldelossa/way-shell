#pragma once

#include <adwaita.h>
#include <upower.h>

G_BEGIN_DECLS

// Service which provides power state information for various devices.
// Backend is provided by UPower daemon.
struct _UPowerService;
#define UPOWER_SERVICE_TYPE upower_service_get_type()
G_DECLARE_FINAL_TYPE(UPowerService, upower_service, UPOWER, SERVICE, GObject);

G_END_DECLS

int upower_service_global_init(void);

UpClient *upower_service_get_up_client(UPowerService *self);

// Get the global upower service
// Will return NULL if `clock_service_global_init` has not been called.
UPowerService *upower_service_get_global();

// Returns the primary power source for the system.
// If a battery is present this will be the first battery on the system.
// If no battery is present this will be the AC line power device.
UpDevice *upower_service_get_primary_device(UPowerService *self);

// Returns whether the primary device is a battery or an AC device.
gboolean upower_service_primary_is_bat(UPowerService *self);

// Returns our application's preferred icon to represent the state of the
// UpDevice.
char *upower_device_map_icon_name(UpDevice *device);