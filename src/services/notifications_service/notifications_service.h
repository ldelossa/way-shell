#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

// A Service which acts as a org.freedesktop.Notification daemon.
//
// Listens on DBUS for notifications and provides an API for acting upon them.
struct _NotificationsService;
#define NOTIFICATIONS_SERVICE_TYPE notifications_service_get_type()
G_DECLARE_FINAL_TYPE(NotificationsService, notifications_service, NOTIFICATIONS,
                     SERVICE, GObject);

G_END_DECLS

enum NotifcationsClosedReason {
    NOTIFICATIONS_CLOSED_REASON_EXPIRED = 1,
    NOTIFICATIONS_CLOSED_REASON_DISMISSED = 2,
    NOTIFICATIONS_CLOSED_REASON_REQUESTED = 3,
    NOTIFICATIONS_CLOSED_REASON_UNDEFINED = 4,
};

typedef struct _NotificationImageData {
    // width (i): Width of image in pixels
    // height (i): Height of image in pixels
    // rowstride (i): Distance in bytes between row starts
    // has_alpha (b): Whether the image has an alpha channel
    // bits_per_sample (i): Must always be 8
    // channels (i): If has_alpha is TRUE, must be 4, otherwise 3
    // data (ay): The image data, in RGB byte order
    uint32_t width;
    uint32_t height;
    uint32_t rowstride;
    gboolean has_alpha;
    uint32_t bits_per_sample;
    uint32_t channels;
    char *data;
} NotificationImageData;

// org.freedesktop.Notification structure
typedef struct _Notification {
    char *app_name;
    char *app_icon;
    char *summary;
    char *body;
    char **actions;
    GVariant *hints;
    char *category;
    char *desktop_entry;
    char *image_path;
    NotificationImageData img_data;
    int32_t id;
    uint32_t replaces_id;
    int32_t expire_timeout;
    gboolean action_icons;
    gboolean resident;
    gboolean transient;
    uint8_t urgency;
    // way-shell fields
    // is_internal informs way-shell this is a notification created by way-shell
    // itself. app_icon will take the form of an icon_name in the default icon
    // theme.
    gboolean is_internal;
    GDateTime *created_on;
} Notification;

int notifications_service_global_init();

NotificationsService *notifications_service_get_global();

int notifications_service_closed_notification(
    NotificationsService *self, guint32 id,
    enum NotifcationsClosedReason reason);

int notifications_service_invoke_action(NotificationsService *self, guint32 id,
                                        char *action_key);

GPtrArray *notifications_service_get_notifications(NotificationsService *self);

// Internal notifications API which can be used by Way-Shell.
// Actions currently not supported, fill in n->app_icon with a themed icon name
// to set the icon to a specific icon.
void notifications_service_send_notification(NotificationsService *self, Notification *n);
