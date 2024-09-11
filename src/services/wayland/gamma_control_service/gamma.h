#pragma once

#include <adwaita.h>

#include "../core.h"
#include "../wlr-gamma-control-unstable-v1.h"

typedef struct _WaylandWLRGammaControl {
    WaylandHeader header;
    struct zwlr_gamma_control_v1 *control;
    WaylandOutput *output;
    uint32_t gamma_size;
    int gamma_table_fd;
    int temperature;
} WaylandWLRGammaControl;

G_BEGIN_DECLS

struct _WaylandGammaControlService;
#define WAYLAND_GAMMA_CONTROL_SERVICE_TYPE \
    wayland_gamma_control_service_get_type()
G_DECLARE_FINAL_TYPE(WaylandGammaControlService, wayland_gamma_control_service,
                     WAYLAND, GAMMA_CONTROL_SERVICE, GObject);

G_END_DECLS

int wayland_gamma_control_service_global_init(
    WaylandCoreService *core, struct zwlr_gamma_control_manager_v1 *mgr);

WaylandGammaControlService *wayland_gamma_control_service_get_global();

void wayland_gamma_control_service_set_temperature(
    WaylandGammaControlService *self, double temperature);

void wayland_gamma_control_service_destroy(WaylandGammaControlService *self);

gboolean wayland_gamma_control_service_enabled(
    WaylandGammaControlService *self);
