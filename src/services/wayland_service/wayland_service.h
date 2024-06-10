#pragma once

#include <adwaita.h>
#include <wayland-client-core.h>
#include <wayland-client.h>

enum WaylandType { WL_REGISTRY, WL_SEAT, WL_OUTPUT };

typedef struct _WaylandHeader {
    enum WaylandType type;
} WaylandHeader;

typedef struct WaylandSeat {
    WaylandHeader header;
    struct wl_seat *seat;
    char *name;
} WaylandSeat;

enum WaylandWLRForeignTopLevelState {
    TOPLEVEL_STATE_MAXIMIZED,
    TOPLEVEL_STATE_MINIMIZED,
    TOPLEVEL_STATE_ACTIVATED,
    TOPLEVEL_STATE_FULLSCEEN,
};

typedef struct _WaylandWLRForeignTopLevel {
    WaylandHeader header;
    struct zwlr_foreign_toplevel_handle_v1 *toplevel;
    char *app_id;
    char *title;
    gboolean entered;
    gboolean activated;
    gboolean closed;
    enum WaylandWLRForeignTopLevelState state;

} WaylandWLRForeignTopLevel;

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

void wayland_wlr_foreign_toplevel_activate(WaylandService *self,
                                           WaylandWLRForeignTopLevel *toplevel);

void wayland_wlr_foreign_toplevel_close(WaylandService *self,
                                        WaylandWLRForeignTopLevel *toplevel);

void wayland_wlr_foreign_toplevel_maximize(WaylandService *self,
                                           WaylandWLRForeignTopLevel *toplevel);

void wayland_wlr_shortcuts_inhibitor_create(WaylandService *self,
                                            GtkWidget *widget);

void wayland_wlr_shortcuts_inhibitor_destroy(WaylandService *self);
