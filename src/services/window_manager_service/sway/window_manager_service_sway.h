#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

struct _WMServiceSway;
#define WM_SERVICE_SWAY_TYPE wm_service_sway_get_type()
G_DECLARE_FINAL_TYPE(WMServiceSway, wm_service_sway, WM_SERVICE, SWAY, GObject);

G_END_DECLS

// Initializes the sway wm service.
int wm_service_sway_global_init(void);

// Get the global wm service
// Will return NULL if `wm_service_sway_global_init` has not been called.
WMServiceSway *wm_service_sway_get_global();

// Returns an array of WMWorkspace pointers representing the current workspaces
// and their state in the window manager.
GPtrArray *wm_service_sway_get_workspaces(WMServiceSway *self);

// Attempts to focus the provided workspace name.
int wm_service_sway_focus_workspace(WMServiceSway *self, gchar *name);
