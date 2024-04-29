#pragma once

#include <adwaita.h>

#include "app_switcher_app_widget.h"

G_BEGIN_DECLS

struct _AppSwitcher;
#define APP_SWITCHER_TYPE app_switcher_get_type()
G_DECLARE_FINAL_TYPE(AppSwitcher, app_switcher, App, Switcher, GObject);

G_END_DECLS

void app_switcher_activate(AdwApplication *app, gpointer user_data);

AppSwitcher *app_switcher_get_global();

void app_switcher_show(AppSwitcher *self);

void app_switcher_hide(AppSwitcher *self);

void app_switcher_toggle(AppSwitcher *self);

void app_switcher_focus_by_app_widget(AppSwitcher *self,
                                      AppSwitcherAppWidget *widget);

GtkWindow *app_switcher_get_window(AppSwitcher *self);

void app_switcher_enter_preview(AppSwitcher *self);

void app_switcher_exit_preview(AppSwitcher *self);

void app_switcher_unfocus_widget_all(AppSwitcher *self);
