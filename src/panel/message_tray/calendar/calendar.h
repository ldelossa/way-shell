#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

struct _Calendar;
#define CALENDAR_TYPE calendar_get_type()
G_DECLARE_FINAL_TYPE(Calendar, calendar, MESSAGE_TRAY, CALENDAR, GObject);

GtkWidget *calendar_get_widget(Calendar *self);

G_END_DECLS