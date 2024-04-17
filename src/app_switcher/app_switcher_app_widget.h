#pragma once

#include <adwaita.h>

#include "./../services/wayland_service/wayland_service.h"

G_BEGIN_DECLS

struct _AppSwitcherAppWidget;
#define APP_SWITCHER_APP_WIDGET_TYPE app_switcher_app_widget_get_type()
G_DECLARE_FINAL_TYPE(AppSwitcherAppWidget, app_switcher_app_widget, AppSwitcher,
                     AppWidget, GObject);

G_END_DECLS

GtkWidget *app_switcher_app_widget_get_widget(AppSwitcherAppWidget *self);

void app_switcher_app_widget_add_toplevel(AppSwitcherAppWidget *self,
                                          WaylandWLRForeignTopLevel *toplevel);

gboolean app_switcher_app_widget_remove_toplevel(
    AppSwitcherAppWidget *self, WaylandWLRForeignTopLevel *toplevel);

gchar *app_switcher_app_widget_get_app_id(AppSwitcherAppWidget *self);

void app_switcher_app_widget_activate(AppSwitcherAppWidget *self);

void app_switcher_app_widget_set_focused(AppSwitcherAppWidget *self);

void app_switcher_app_widget_set_focused_next_instance(
    AppSwitcherAppWidget *self);

void app_switcher_app_widget_set_focused_prev_instance(
    AppSwitcherAppWidget *self);

void app_switcher_app_widget_unset_focus(AppSwitcherAppWidget *self);

void app_switcher_app_widget_update_last_activated(
    AppSwitcherAppWidget *self, WaylandWLRForeignTopLevel *toplevel);

