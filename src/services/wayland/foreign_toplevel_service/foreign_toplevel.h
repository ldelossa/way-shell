#pragma once

#include <adwaita.h>

#include "../core.h"
#include "../wlr-foreign-toplevel-management-unstable-v1.h"

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

G_BEGIN_DECLS

struct _WaylandForeignToplevelService;
#define WAYLAND_FOREIGN_TOPLEVEL_SERVICE_TYPE \
    wayland_foreign_toplevel_service_get_type()
G_DECLARE_FINAL_TYPE(WaylandForeignToplevelService,
                     wayland_foreign_toplevel_service, WAYLAND,
                     FOREIGN_TOPLEVEL_SERVICE, GObject);

G_END_DECLS

int wayland_foreign_toplevel_service_global_init(
    WaylandCoreService *core, struct zwlr_foreign_toplevel_manager_v1 *mgr);

WaylandForeignToplevelService *wayland_foreign_toplevel_service_get_global();

GHashTable *wayland_foreign_toplevel_service_get_toplevels(
    WaylandForeignToplevelService *self);

void wayland_foreign_toplevel_service_activate(
    WaylandForeignToplevelService *self, WaylandWLRForeignTopLevel *toplevel);

void wayland_foreign_toplevel_service_close(
    WaylandForeignToplevelService *self, WaylandWLRForeignTopLevel *toplevel);

void wayland_foreign_toplevel_service_maximize(
    WaylandForeignToplevelService *self, WaylandWLRForeignTopLevel *toplevel);
