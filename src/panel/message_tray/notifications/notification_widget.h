#pragma once

#include <adwaita.h>

#include "../../../services/notifications_service/notifications_service.h"

typedef struct _NotificationWidget {
    guint32 id;
    GtkEventControllerMotion *ctrl;
    GtkBox *container;
    GtkButton *button;
    GtkImage *icon;
    GtkOverlay *overlay;
    GtkButton *dismiss;
    GtkRevealer *dismiss_revealer;
    GtkLabel *summary;
    GtkLabel *body;
} NotificationWidget;

NotificationWidget *notification_widget_from_notification(Notification *n);

void notification_widget_free(NotificationWidget *w);