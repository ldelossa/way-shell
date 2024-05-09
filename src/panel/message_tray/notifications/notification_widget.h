#pragma once

#include <adwaita.h>

#include "../../../services/media_player_service/media_player_service.h"
#include "../../../services/notifications_service/notifications_service.h"

G_BEGIN_DECLS

struct _NotificationWidget;
#define NOTIFICATION_WIDGET_TYPE notification_widget_get_type()
G_DECLARE_FINAL_TYPE(NotificationWidget, notification_widget, NOTIFICATION,
                     WIDGET, GObject);

G_END_DECLS

NotificationWidget *notification_widget_from_notification(
    Notification *n, gboolean expand_on_enter);

NotificationWidget *notification_widget_from_media_player(MediaPlayer *player);

NotificationWidget *notification_widget_set_media_player(
    NotificationWidget *self, MediaPlayer *player);

GtkWidget *notification_widget_get_widget(NotificationWidget *self);

guint32 notification_widget_get_id(NotificationWidget *self);

GtkLabel *notification_widget_get_summary(NotificationWidget *self);

GtkLabel *notification_widget_get_body(NotificationWidget *self);

void notification_widget_set_stack_effect(NotificationWidget *self,
                                          gboolean show);

void notification_widget_dismiss_notification(NotificationWidget *self);

gchar *notification_widget_get_media_player_name(NotificationWidget *self);
