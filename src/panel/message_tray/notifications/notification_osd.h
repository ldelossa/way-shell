#pragma once

#include <adwaita.h>

#include "./notification_widget.h"
#include "./notifications_list.h"

G_BEGIN_DECLS

struct _NotificationsOSD;
#define NOTIFICATIONS_OSD_TYPE notifications_osd_get_type()
G_DECLARE_FINAL_TYPE(NotificationsOSD, notifications_osd, NOTIFICATIONS, OSD,
                     GObject);

G_END_DECLS

void notification_osd_reinitialize(NotificationsOSD *self);

void notification_osd_set_notification_list(NotificationsOSD *self,
                                            NotificationsList *list);

void notification_osd_connect_message_tray_signals(NotificationsOSD *self,
                                                   MessageTray *tray);
