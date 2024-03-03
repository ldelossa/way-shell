#pragma once

#include <adwaita.h>

enum ThemeServiceTheme { THEME_LIGHT, THEME_DARK };

G_BEGIN_DECLS

struct _ThemeService;
#define THEME_SERVICE_TYPE theme_service_get_type()
G_DECLARE_FINAL_TYPE(ThemeService, theme_service, THEME, SERVICE, GObject);

G_END_DECLS

int theme_service_global_init(void);

ThemeService *theme_service_get_global();

void theme_service_set_light_theme(ThemeService *self, gboolean light_theme);

void theme_service_set_dark_theme(ThemeService *self, gboolean dark_theme);

enum ThemeServiceTheme theme_service_get_theme(ThemeService *self);
