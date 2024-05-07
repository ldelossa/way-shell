#pragma once

#include <adwaita.h>

#include "../../../services/notifications_service/notifications_service.h"

G_BEGIN_DECLS

struct _NotificationGroup;
#define NOTIFICATION_GROUP_TYPE notification_group_get_type()
G_DECLARE_FINAL_TYPE(NotificationGroup, notification_group, NOTIFICATION, GROUP,
                     GObject);

G_END_DECLS

GtkWidget *notification_group_get_widget(NotificationGroup *self);

void notification_group_add_notification(NotificationGroup *self,
                                         Notification *n);

gchar *notification_group_get_app_name(NotificationGroup *self);

void notification_group_dismiss_all(NotificationGroup *self);
