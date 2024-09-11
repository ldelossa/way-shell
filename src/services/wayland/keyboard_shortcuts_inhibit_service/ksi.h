#pragma once

#include <adwaita.h>

#include "../core.h"

G_BEGIN_DECLS

// The WaylandKSIService can be used to black Wayland's registered keyboard
// shortcuts and forward them to the GdkToplevel provided to its API.
//
// A GdkToplevel is the top-most GdkWidget, which is usually the Gdk/AdwWindow
// widget.
//
// For simplicity, only a single GdkToplevel can own the inhibitor at a time.
// This maybe expanded if the need for multiple GdkToplevels to own the
// inhibitor arises.
struct _WaylandKSIService;
#define WAYLAND_KSI_SERVICE_TYPE wayland_ksi_service_get_type()
G_DECLARE_FINAL_TYPE(WaylandKSIService, wayland_ksi_service, WAYLAND,
                     KSI_SERVICE, GObject);

G_END_DECLS

int wayland_ksi_service_global_init(WaylandCoreService *core);

WaylandKSIService *wayland_ksi_service_get_global();

// Make compositor forward all key events to `widget`.
// `widget` must be a GdkToplevel.
gboolean wayland_ksi_inhibit(WaylandKSIService *self, GtkWidget *widget);

// Destroy the key events inhibitor, if it exists.
gboolean wayland_ksi_inhibit_destroy(WaylandKSIService *self);
