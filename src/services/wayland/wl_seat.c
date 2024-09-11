#include "wl_seat.h"

#include <unistd.h>

#define WAYLAND_SEAT(wl_seat)                     \
    self = (WaylandCoreService *)data;            \
    seats = wayland_core_service_get_seats(self); \
    seat_data = g_hash_table_lookup(seats, seat);

static void wl_seat_listener_capabilities(void *data, struct wl_seat *seat,
                                          uint32_t capabilities) {
    g_debug("wl_seat.c:seat_listener_capabilities(): capabilities: %d",
            capabilities);
    // no-op, we don't care about capabilities
}

static void wl_seat_listener_name(void *data, struct wl_seat *seat,
                                  const char *name) {
    g_debug("wl_seat.c:seat_listener_name(): name: %s", name);

    WaylandCoreService *self;
    GHashTable *seats;
    WaylandSeat *seat_data;
    WAYLAND_SEAT(seat);

    seat_data->name = g_strdup(name);

    g_debug("wl_seat.c:seat_listener_name(): seat_data->name: %s",
            seat_data->name);
}

static const struct wl_seat_listener wl_seat_listener = {
    .capabilities = wl_seat_listener_capabilities,
    .name = wl_seat_listener_name,
};

void wl_seat_register(WaylandCoreService *self, struct wl_seat *wl_seat,
                      uint32_t name) {
    GHashTable *seats = wayland_core_service_get_seats(self);
    GHashTable *globals = wayland_core_service_get_globals(self);

    WaylandSeat *seat = g_new0(WaylandSeat, 1);
    seat->header.type = WL_SEAT;
    seat->seat = wl_seat;
    seat->name = NULL;

    g_hash_table_insert(globals, GUINT_TO_POINTER(name), seat);
    g_hash_table_insert(seats, wl_seat, seat);
    wl_seat_add_listener(wl_seat, &wl_seat_listener, self);
}

void wl_seat_remove(WaylandCoreService *self, WaylandSeat *seat) {
    g_debug("wl_seat.c:wayland_seat_remove(): seat: %s", seat->name);
    GHashTable *seats = wayland_core_service_get_seats(self);
    GHashTable *globals = wayland_core_service_get_globals(self);

    // remove from seats hash table
    g_hash_table_remove(seats, seat->seat);

    // release seat from wayland
    wl_seat_release(seat->seat);

    // free seat
    g_free(seat->name);
    g_free(seat);

    g_hash_table_remove(globals, GUINT_TO_POINTER(seat->name));
}
