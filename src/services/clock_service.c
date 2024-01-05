#include "clock_service.h"

#include <adwaita.h>

static ClockService *global = NULL;

enum signals { tick, signals_n };

struct _ClockService {
    GObject parent_instance;
    gboolean enabled;
};
static guint signals[signals_n] = {0};
G_DEFINE_TYPE(ClockService, clock_service, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init methods
static void clock_service_dispose(GObject *gobject) {
    ClockService *self = CLOCK_SERVICE(gobject);

    // Chain-up
    G_OBJECT_CLASS(clock_service_parent_class)->dispose(gobject);
};

static void clock_service_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(clock_service_parent_class)->finalize(gobject);
};

static void clock_service_class_init(ClockServiceClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = clock_service_dispose;
    object_class->finalize = clock_service_finalize;

    // create a signal which has a GDateTime as a argument
    signals[tick] = g_signal_new("tick", G_TYPE_FROM_CLASS(object_class),
                                       G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL,
                                       G_TYPE_NONE, 1, G_TYPE_DATE_TIME);
};

static gboolean emit_tick(gpointer self) {
    GDateTime *now = g_date_time_new_now_local();
    g_debug("clock_service.c:emit_tick() emitting signal.");
    g_signal_emit(self, signals[tick], 0, now);
    g_date_time_unref(now);
    return CLOCK_SERVICE(self)->enabled;
}

// syncs the click to the first minute boundary.
static gboolean tick_sync(gpointer self) {
    GDateTime *now = g_date_time_new_now_local();
    if (g_date_time_get_second(now) != 0) {
        g_date_time_unref(now);
        return CLOCK_SERVICE(self)->enabled;
    }
    g_timeout_add_seconds_full(G_PRIORITY_DEFAULT, 60, emit_tick, self, NULL);
    g_debug("clock_service.c:tick_sync() synchronized on minute boundary");
    return false;
}

static void clock_service_init(ClockService *self){
    self->enabled = TRUE;
    emit_tick(self);
    g_timeout_add(500, tick_sync, self);
};

int clock_service_global_init(void) {
    global = g_object_new(CLOCK_SERVICE_TYPE, NULL);
    return 0;
}

ClockService *clock_service_get_global() {
    return global;
}