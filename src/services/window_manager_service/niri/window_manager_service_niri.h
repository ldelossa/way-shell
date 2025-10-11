#pragma once

#include <adwaita.h>

#include "../window_manager_service.h"

G_BEGIN_DECLS

struct _WMServiceNiri;
#define WM_SERVICE_NIRI_TYPE wm_service_niri_get_type()
G_DECLARE_FINAL_TYPE(WMServiceNiri, wm_service_niri, WM_SERVICE, NIRI, GObject);

G_END_DECLS

WindowManager *wm_service_niri_window_manager_init();
