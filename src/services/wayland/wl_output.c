#include "core.h"

#define WAYLAND_OUTPUT(wl_output)                     \
    self = (WaylandCoreService *)data;                \
    outputs = wayland_core_service_get_outputs(self); \
    output_data = g_hash_table_lookup(outputs, output)

static void wl_output_handle_description(void *data, struct wl_output *output,
                                         const char *description) {
    g_debug("wl_output.c:wl_output_handle_description(): description: %s",
            description);

    WaylandCoreService *self;
    GHashTable *outputs;
    WaylandOutput *output_data;
    WAYLAND_OUTPUT(output);

    output_data->desc = g_strdup(description);

    g_debug(
        "wl_output.c:wl_output_handle_description(): output_data->desc: "
        "%s",
        output_data->desc);
}

static void wl_output_handle_done(void *data, struct wl_output *output) {
    g_debug("wl_output.c:wl_output_handle_done(): output done");

    WaylandCoreService *self;
    GHashTable *outputs;
    WaylandOutput *output_data;
    WAYLAND_OUTPUT(output);

    if (!output_data) {
        return;
    }

    output_data->initialized = TRUE;

    g_signal_emit(self, wayland_core_service_signals[wl_output_added], 0,
                  outputs, output_data);

    g_debug(
        "wl_output.c:wl_output_handle_done(): output_data->name: %s, "
        "output_data->desc: %s, output_data->make: %s, output_data->model: %s",
        output_data->name, output_data->desc, output_data->make,
        output_data->model);
}

static void wl_output_handle_geometry(void *data, struct wl_output *output,
                                      int32_t x, int32_t y,
                                      int32_t physical_width,
                                      int32_t physical_height, int32_t subpixel,
                                      const char *make, const char *model,
                                      int32_t transform) {
    g_debug(
        "wl_output.c:wl_output_handle_geometry(): x: %d, y: %d, "
        "physical_width: %d, physical_height: %d, subpixel: %d, make: %s, "
        "model: %s, transform: %d",
        x, y, physical_width, physical_height, subpixel, make, model,
        transform);

    WaylandCoreService *self;
    GHashTable *outputs;
    WaylandOutput *output_data;
    WAYLAND_OUTPUT(output);

    output_data->make = g_strdup(make);
    output_data->model = g_strdup(model);

    g_debug(
        "wl_output.c:wl_output_handle_geometry(): output_data->make: %s, "
        "output_data->model: %s",
        output_data->make, output_data->model);
}

static void wl_output_handle_mode(void *data, struct wl_output *output,
                                  uint32_t flags, int32_t width, int32_t height,
                                  int32_t refresh) {
    g_debug(
        "wl_output.c:wl_output_handle_mode(): flags: %d, width: %d, "
        "height: %d, refresh: %d",
        flags, width, height, refresh);
}

static void wl_output_handle_name(void *data, struct wl_output *output,
                                  const char *name) {
    g_debug("wl_output.c:wl_output_handle_name(): name: %s", name);

    WaylandCoreService *self;
    GHashTable *outputs;
    WaylandOutput *output_data;
    WAYLAND_OUTPUT(output);

    output_data->name = g_strdup(name);

    g_debug("wl_output.c:wl_output_handle_name(): output_data->name: %s",
            output_data->name);
}

static void wl_output_handle_scale(void *data, struct wl_output *output,
                                   int32_t scale) {
    g_debug("wl_output.c:wl_output_handle_scale(): scale: %d", scale);
}

static const struct wl_output_listener wl_output_listener = {
    .description = wl_output_handle_description,
    .done = wl_output_handle_done,
    .geometry = wl_output_handle_geometry,
    .mode = wl_output_handle_mode,
    .name = wl_output_handle_name,
    .scale = wl_output_handle_scale,
};

void wl_output_register(WaylandCoreService *self, struct wl_output *wl_output,
                        uint32_t name) {
    GHashTable *outputs = wayland_core_service_get_outputs(self);
    GHashTable *globals = wayland_core_service_get_globals(self);

    WaylandOutput *output = g_new0(WaylandOutput, 1);
    output->header.type = WL_OUTPUT;
    output->output = wl_output;
    output->name = NULL;
    output->desc = NULL;
    output->make = NULL;
    output->model = NULL;
    output->initialized = FALSE;

    g_hash_table_insert(outputs, wl_output, output);
    g_hash_table_insert(globals, GUINT_TO_POINTER(name), output);
    wl_output_add_listener(wl_output, &wl_output_listener, self);
}

void wl_output_remove(WaylandCoreService *self, WaylandOutput *output) {
    g_debug("wl_output.c:wayland_output_remove(): output: %s",
            output->name);
    GHashTable *outputs = wayland_core_service_get_outputs(self);
    GHashTable *globals = wayland_core_service_get_globals(self);

    // remove from outputs hash table
    g_hash_table_remove(outputs, output->output);

    // release output from wayland
    wl_output_release(output->output);

    // free output
    g_free(output->name);
    g_free(output->desc);
    g_free(output->make);
    g_free(output->model);
    g_free(output);

    g_hash_table_remove(globals, GUINT_TO_POINTER(output->name));
}
