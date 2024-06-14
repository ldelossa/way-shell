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

void brightness_service_backlight_up(BrightnessService *self);

void brightness_service_backlight_down(BrightnessService *self);

void brightness_service_set_backlight(BrightnessService *self, float percent);

float brightness_service_get_backlight(BrightnessService *self);

void brightness_service_keyboard_up(BrightnessService *self);

void brightness_service_keyboard_down(BrightnessService *self);

void brightness_service_set_keyboard(BrightnessService *self, uint32_t value);

guint32 brightness_service_get_keyboard(BrightnessService *self);

guint32 brightness_service_get_keyboard_max(BrightnessService *self);

gchar *brightness_service_map_icon(BrightnessService *self);

gboolean brightness_service_has_backlight_brightness(BrightnessService *self);

gboolean brightness_service_has_keyboard_brightness(BrightnessService *self);
