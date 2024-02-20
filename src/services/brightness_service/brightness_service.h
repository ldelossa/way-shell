#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

struct _BrightnessService;
#define BRIGHTNESS_SERVICE_TYPE brightness_service_get_type()
G_DECLARE_FINAL_TYPE(BrightnessService, brightness_service, BRIGHTNESS, SERVICE,
                     GObject);

G_END_DECLS

int brightness_service_global_init(void);

// Get the global brightness service
// Will return NULL if `brightness_service_global_init` has not been called.
BrightnessService *brightness_service_get_global();

void brightness_service_up(BrightnessService *self);

void brightness_service_down(BrightnessService *self);