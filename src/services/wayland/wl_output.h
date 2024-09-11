#pragma once

#include "core.h"

void wl_output_register(WaylandCoreService *self, struct wl_output *wl_output, uint32_t name);

void wl_output_remove(WaylandCoreService *self, WaylandOutput *output);
