#pragma once

#include <adwaita.h>
#include <wayland-client-core.h>
#include <wayland-client.h>

enum WaylandType { WL_REGISTRY, WL_OUTPUT_DB, WL_OUTPUT };

typedef struct _WaylandHeader {
    enum WaylandType type;
} WaylandHeader;

typedef struct _WaylandOutputDB {
    WaylandHeader header;
    GHashTable *db;
} WaylandOutputDB;

// domain representation of a wl_output.
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

struct _WaylandService;
#define WAYLAND_SERVICE_TYPE wayland_service_get_type()
G_DECLARE_FINAL_TYPE(WaylandService, wayland_service, WAYLAND, SERVICE,
                     GObject);

G_END_DECLS

int wayland_service_global_init(void);

// Will return NULL if `wayland_service_global_init` has not been called.
WaylandService *wayland_service_get_global();
