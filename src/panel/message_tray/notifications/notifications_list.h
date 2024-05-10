#pragma once

#include <adwaita.h>

#include "../message_tray.h"

G_BEGIN_DECLS

struct _NotificationsList;
#define NOTIFICATIONS_LIST_TYPE notifications_list_get_type()
G_DECLARE_FINAL_TYPE(NotificationsList, notifications_list, NOTIFICATIONS, LIST,
                     GObject);

G_END_DECLS

GtkWidget *notifications_list_get_widget(NotificationsList *self);

gboolean notifications_list_is_dnd(NotificationsList *self);

void notifications_list_reinitialize(NotificationsList *self);

void notification_list_connect_message_tray_signals(NotificationsList *self,
                                                    MessageTray *tray);
