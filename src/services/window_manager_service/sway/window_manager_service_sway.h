#pragma once

#include <adwaita.h>

#include "../window_manager_service.h"

G_BEGIN_DECLS

struct _WMServiceSway;
#define WM_SERVICE_SWAY_TYPE wm_service_sway_get_type()
G_DECLARE_FINAL_TYPE(WMServiceSway, wm_service_sway, WM_SERVICE, SWAY, GObject);

G_END_DECLS

WindowManager *wm_service_sway_window_manager_init();

