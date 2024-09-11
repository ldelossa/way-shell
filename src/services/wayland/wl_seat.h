#pragma once

#include "core.h"

void wl_seat_register(WaylandCoreService *self, struct wl_seat *wl_seat,
                      uint32_t name);

void wl_seat_remove(WaylandCoreService *self, WaylandSeat *seat);
