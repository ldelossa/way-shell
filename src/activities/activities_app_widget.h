#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

struct _ActivitesAppWidget;
#define ACTIVITIES_APP_WIDGET_TYPE activities_app_widget_get_type()
G_DECLARE_FINAL_TYPE(ActivitiesAppWidget, activities_app_widget, Activities,
                     AppWidget, GObject);

G_END_DECLS

void activities_app_widget_set_app_info(ActivitiesAppWidget *self,
                                        GAppInfo *app_info);

GAppInfo *activities_app_widget_get_app_info(ActivitiesAppWidget *self);

void activities_app_widget_simulate_click(ActivitiesAppWidget *self);

GtkWidget *activities_app_widget(ActivitiesAppWidget *self);

ActivitiesAppWidget *activities_app_widget_from_widget(GtkWidget *widget);
