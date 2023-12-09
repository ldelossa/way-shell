#include <adwaita.h>

G_BEGIN_DECLS

// Simple clock service which emits a 'tick' signal on every minute.
// The clock is synchronized to the next minute boundary following its
// construction.
//
// `tick` event provides a GDateTime as its first argument.
struct _ClockService;
#define CLOCK_SERVICE_TYPE clock_service_get_type()
G_DECLARE_FINAL_TYPE(ClockService, clock_service, CLOCK, SERVICE, GObject);

G_END_DECLS

int clock_service_global_init(void);

// Get the global clock service
// Will return NULL if `clock_service_global_init` has not been called.
ClockService *clock_service_get_global();

gboolean clock_service_get_enabled(ClockService *self);

// Once a clock service is disabled you can discard it.
// Create a new clock service if you need an enabled clock tick.
gboolean clock_service_set_enabled(ClockService *self, gboolean enabled);