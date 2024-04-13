#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

struct _Activities;
#define ACTIVITIES_TYPE activities_get_type()
G_DECLARE_FINAL_TYPE(Activities, activities, Activities, Activities, GObject);

G_END_DECLS

void activites_reinitialize(Activities *self);

void activities_activate(AdwApplication *app, gpointer user_data);

Activities *activities_get_global();

void activities_show(Activities *self);

void activities_hide(Activities *self);

void activities_toggle(Activities *self);

