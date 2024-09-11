#pragma once

#include <adwaita.h>
#include <wayland-client-core.h>
#include <wayland-client.h>

// expose WaylandCoreService's signals in our header so we can allow invoking
// signals from global listener handlers in separate files. See: wl_output.c for
// an example.
enum wl_signals { wl_output_added, wl_output_removed, wl_signals_n };

extern guint wayland_core_service_signals[wl_signals_n];

enum WaylandType {
    WL_REGISTRY,
    WL_SEAT,
    WL_OUTPUT,
    WLR_FOREIGN_TOPLEVEL,
    WLR_GAMMA_CONTROL
};

typedef struct _WaylandHeader {
    enum WaylandType type;
} WaylandHeader;

typedef struct WaylandSeat {
    WaylandHeader header;
    struct wl_seat *seat;
    char *name;
} WaylandSeat;

typedef struct _WaylandOutput {
    WaylandHeader header;
    struct wl_output *output;
    GdkMonitor *monitor;
    char *name;
    char *desc;
    char *make;
    char *model;
    gboolean initialized;
} WaylandOutput;

G_BEGIN_DECLS

// The WaylandCoreService implements the core Wayland interface.
//
// This includes inventorying useful core wayland protocol objects along with
// instantiating useful services if the global managers and protocols are
// available.
//
// As a separation of concerns, WaylandCoreService is responsible for binding to
// useful globals, at which point the bound interface is then handed to any
// instantiated service which implements the interface's functionality.
//
// In practice, this means the WaylandCoreService inventories and signals when
// https://wayland.app/protocols/wayland objects are added or removed.
//
// Additionally, it will query the inventory for useful globals and construct
// Way-Shell services for these globals. For instance, if WaylandCoreService
// discovers that `zwlr_gamma_control_manager_v1` is available it will construct
// a WaylandGammaControlService and this service's API can be used to implement
// a 'nightlight' feature.
//
// Operationally, this service's initialization blocks the main thread until an
// initial sync with the Wayland server,  guaranteeing that all other Wayland
// related services are instantiated by the time the core service's
// instantiation returns.
struct _WaylandCoreService;
#define WAYLAND_CORE_SERVICE_TYPE wayland_core_service_get_type()
G_DECLARE_FINAL_TYPE(WaylandCoreService, wayland_core_service, WAYLAND,
                     CORE_SERVICE, GObject);

G_END_DECLS

int wayland_core_service_global_init(void);

WaylandCoreService *wayland_core_service_get_global();

GHashTable *wayland_core_service_get_globals(WaylandCoreService *self);

GHashTable *wayland_core_service_get_seats(WaylandCoreService *self);

GHashTable *wayland_core_service_get_outputs(WaylandCoreService *self);

struct wl_display *wayland_core_service_get_display(WaylandCoreService *self);
