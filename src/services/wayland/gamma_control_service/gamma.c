#include "gamma.h"

#include <adwaita.h>
#include <fcntl.h> /* For O_* constants */
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */

#include "../core.h"
#include "colorramp.h"

enum signals { gamma_control_enabled, gamma_control_disabled, signals_n };

static WaylandGammaControlService *global = NULL;

struct _WaylandGammaControlService {
    GObject parent_instance;
    WaylandCoreService *core;
    // wlr gamma control manager for adjusting gamma
    struct zwlr_gamma_control_manager_v1 *mgr;
    // Whether gamma is being controlled
    gboolean enabled;
    // Table of created gamma controllers keyed by *WaylandOuput
    GHashTable *controllers_by_output;
    // Table of created gamma controllers keyed by their `zwlr_gamma_control_v1`
    // proxy interfaces.
    GHashTable *controllers_by_proxy;
    // The last seen temperature
    double temperature;
};

static guint service_signals[signals_n] = {0};

G_DEFINE_TYPE(WaylandGammaControlService, wayland_gamma_control_service,
              G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init methods for this GObject
static void wayland_gamma_control_service_dispose(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(wayland_gamma_control_service_parent_class)
        ->dispose(gobject);
};

static void wayland_gamma_control_service_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(wayland_gamma_control_service_parent_class)
        ->finalize(gobject);
};

static void wayland_gamma_control_service_class_init(
    WaylandGammaControlServiceClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = wayland_gamma_control_service_dispose;
    object_class->finalize = wayland_gamma_control_service_finalize;

    service_signals[gamma_control_enabled] =
        g_signal_new("gamma-control-enabled", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    service_signals[gamma_control_disabled] =
        g_signal_new("gamma-control-disabled", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
};

static void wayland_gamma_control_service_init(
    WaylandGammaControlService *self) {
    self->enabled = false;
    self->controllers_by_output =
        g_hash_table_new(g_direct_hash, g_direct_equal);
    self->controllers_by_proxy =
        g_hash_table_new(g_direct_hash, g_direct_equal);
};

int wayland_gamma_control_service_global_init(
    WaylandCoreService *core, struct zwlr_gamma_control_manager_v1 *mgr) {
    global = g_object_new(WAYLAND_GAMMA_CONTROL_SERVICE_TYPE, NULL);
    global->core = core;
    global->mgr = mgr;
    return 0;
};

static void apply_gamma_control(WaylandGammaControlService *self,
                                WaylandWLRGammaControl *ctrl) {
    g_debug("gamma.c:wayland_wlr_gamma_control_apply() called");
    // when we get here we must have the gamma ramp table size known
    if (ctrl->gamma_size == 0) return;

    // get the total size needed for the gamma table.
    size_t table_size = ctrl->gamma_size * 3 * sizeof(uint16_t);

    // create unique name for our shared memory gamma table.
    char shm_name[255];
    snprintf(shm_name, 255, "/way-shell-ephemeral-gamma-table-%d", rand());

    // we'll create a memory backed fd and share with Wayland.
    int fd = shm_open(shm_name, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        g_error(
            "gamma.c:wayland_wlr_gamma_control_apply() shm_open "
            "failed: %s",
            strerror(errno));
    }

    if (ftruncate(fd, table_size) < 0) {
        g_error(
            "gamma.c:wayland_wlr_gamma_control_apply() ftruncate "
            "failed: %s",
            strerror(errno));
    }

    uint16_t *table =
        mmap(NULL, table_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (table == MAP_FAILED) {
        g_error(
            "gamma.c:wayland_wlr_gamma_control_apply() mmap failed: "
            "%s",
            strerror(errno));
    }

    // fill gammma table.
    uint16_t *r = table;
    uint16_t *g = table + ctrl->gamma_size;
    uint16_t *b = table + (ctrl->gamma_size * 2);

    /* Initialize gamma ramps to pure state */
    for (int i = 0; i < ctrl->gamma_size; i++) {
        uint16_t value = (double)i / ctrl->gamma_size * (UINT16_MAX + 1);
        r[i] = value;
        g[i] = value;
        b[i] = value;
    }

    colorramp_fill(r, g, b, ctrl->gamma_size, ctrl->temperature);

    zwlr_gamma_control_v1_set_gamma(ctrl->control, fd);

    munmap(table, table_size);
    shm_unlink(shm_name);
    close(fd);

    ctrl->gamma_table_fd = fd;
}

static const struct zwlr_gamma_control_v1_listener gamma_control_listener;

static void on_wl_output_added(WaylandCoreService *core, GHashTable *outputs,
                               WaylandOutput *output,
                               WaylandGammaControlService *self) {
    g_debug("gamma.c:on_wl_output_added(): called");
    WaylandWLRGammaControl *ctrl = g_malloc0(sizeof(WaylandWLRGammaControl));

    ctrl->header.type = WLR_GAMMA_CONTROL;
    ctrl->output = output;
    ctrl->temperature = self->temperature;
    ctrl->gamma_size = 0;
    ctrl->gamma_table_fd = -1;
    ctrl->control = zwlr_gamma_control_manager_v1_get_gamma_control(
        self->mgr, output->output);

    g_debug("gamma.c:on_wl_output_added(): ctrl->output: %p", ctrl->output);

    g_hash_table_insert(self->controllers_by_output, ctrl->output, ctrl);
    g_hash_table_insert(self->controllers_by_proxy, ctrl->control, ctrl);

    // add listener for gamma controller
    zwlr_gamma_control_v1_add_listener(ctrl->control, &gamma_control_listener,
                                       self);
}

static void on_wl_output_removed(WaylandCoreService *core,
                                 WaylandOutput *output,
                                 WaylandGammaControlService *self) {
    g_debug("gamma.c:on_wl_output_removed(): called");
    WaylandWLRGammaControl *ctrl =
        g_hash_table_lookup(self->controllers_by_output, output);

    g_debug("gamma.c:on_wl_output_removed(): ctrl->output: %p", ctrl->output);

    if (!ctrl) return;

    g_hash_table_remove(self->controllers_by_output, output);
    g_hash_table_remove(self->controllers_by_proxy, ctrl->control);

    zwlr_gamma_control_v1_destroy(ctrl->control);
    wl_display_flush(wayland_core_service_get_display(self->core));

    g_free(ctrl);
}

void wayland_gamma_control_service_set_temperature(
    WaylandGammaControlService *self, double temperature) {
    g_debug(
        "wayland_gamma_control_service_set_temperature(): called temperature: "
        "%f",
        temperature);

    self->temperature = temperature;

    // if we are already enabled, apply gamma change to all existing controllers
    if (self->enabled) {
        for (GList *l = g_hash_table_get_values(self->controllers_by_output); l;
             l = l->next) {
            WaylandWLRGammaControl *ctrl = l->data;
            ctrl->temperature = temperature;
            apply_gamma_control(self, ctrl);
        }
        wl_display_flush(wayland_core_service_get_display(self->core));
        return;
    }

    // iterate over all monitors and create controllers
    GList *outputs =
        g_hash_table_get_values(wayland_core_service_get_outputs(self->core));
    for (GList *l = outputs; l; l = l->next) {
        WaylandOutput *output = l->data;
        on_wl_output_added(self->core,
                           wayland_core_service_get_outputs(self->core), output,
                           self);
    }

    g_signal_connect(self->core, "wl-output-added",
                     G_CALLBACK(on_wl_output_added), self);
    g_signal_connect(self->core, "wl-output-removed",
                     G_CALLBACK(on_wl_output_removed), self);

    self->enabled = true;
    g_signal_emit(self, service_signals[gamma_control_enabled], 0);
    wl_display_flush(wayland_core_service_get_display(self->core));
}

void wayland_gamma_control_service_destroy(WaylandGammaControlService *self) {
    g_debug("gamma.c:wayland_gamma_control_service_destroy(): called");
    if (!self->enabled) return;

    g_debug(
        "gamma.c:wayland_gamma_control_service_destroy(): destroying gamma "
        "control service");

    for (GList *l = g_hash_table_get_values(self->controllers_by_output); l;
         l = l->next) {
        WaylandWLRGammaControl *ctrl = l->data;
        zwlr_gamma_control_v1_destroy(ctrl->control);
        g_free(ctrl);
    }

    g_hash_table_remove_all(self->controllers_by_output);
    g_hash_table_remove_all(self->controllers_by_proxy);

    wl_display_flush(wayland_core_service_get_display(self->core));

    self->enabled = false;

    g_signal_handlers_disconnect_by_func(self->core, on_wl_output_added, self);
    g_signal_handlers_disconnect_by_func(self->core, on_wl_output_removed,
                                         self);

    g_signal_emit(self, service_signals[gamma_control_disabled], 0);
}

gboolean wayland_gamma_control_service_enabled(
    WaylandGammaControlService *self) {
    return self->enabled;
}

WaylandGammaControlService *wayland_gamma_control_service_get_global() {
    return global;
};

// 							//
// gamma control listener	//
//							//
static void zwlr_gamma_control_handle_size(
    void *data, struct zwlr_gamma_control_v1 *control, uint32_t size) {
    g_debug("gamma.c:zwlr_gamma_control_handle_size(): size: %d", size);

    WaylandGammaControlService *self = (WaylandGammaControlService *)data;

    WaylandWLRGammaControl *ctrl =
        g_hash_table_lookup(self->controllers_by_proxy, control);

    if (!ctrl) return;

    ctrl->gamma_size = size;

    if (self->enabled) {
        apply_gamma_control(self, ctrl);
    }

    wl_display_flush(wayland_core_service_get_display(self->core));

    g_debug(
        "gamma.c:zwlr_gamma_control_handle_size(): ctrl->gamma_size: "
        "%d",
        ctrl->gamma_size);
}

static void zlwr_gamma_control_handle_failed(
    void *data, struct zwlr_gamma_control_v1 *ctrl) {
    g_debug("gamma.c:zlwr_gamma_control_handle_failed(): failed");
    WaylandGammaControlService *self = (WaylandGammaControlService *)data;

    WaylandWLRGammaControl *g =
        g_hash_table_lookup(self->controllers_by_proxy, ctrl);

    if (g) {
        g_hash_table_remove(self->controllers_by_output, g->output);
        g_hash_table_remove(self->controllers_by_proxy, ctrl);
    }

    zwlr_gamma_control_v1_destroy(ctrl);

    wl_display_flush(wayland_core_service_get_display(self->core));
}

static const struct zwlr_gamma_control_v1_listener gamma_control_listener = {
    .gamma_size = zwlr_gamma_control_handle_size,
    .failed = zlwr_gamma_control_handle_failed,
};
