#pragma once

#include <adwaita.h>

#include "dbusmenu_dbus.h"
#include "status_notifier_item_dbus.h"

#define SNI_GACTION_PREFIX "sni"
#define SNI_GRACTION_ITEM_CLICKED "sni.item-clicked"
#define SNI_GRACTION_MENU_ABOUT_TO_SHOW "sni.about-to-show"

typedef struct StatusNotifierItem {
    DbusItemV0Gen *proxy;
    DbusDbusmenu *menu_proxy;
    GActionGroup *action_group;
    GMenu *menu_model;
    gchar *bus_name;
    gchar *obj_name;
    gchar *register_service_name;

    // item properties
    gchar *category;
    gchar *id;
    gchar *title;
    gchar *status;
    guint window_id;

    GdkPixbuf *icon_pixmap_from_theme;
    gchar *icon_theme_path;
    gchar *icon_name;
    GdkPixbuf *icon_pixmap;

    gchar *overlay_icon_name;
    GdkPixbuf *overlay_icon_pixmap;

    gchar *attention_icon_name;
    GdkPixbuf *attention_icon_pixmap;
    gchar *attention_movie_name;
} StatusNotifierItem;

void status_notifier_item_about_to_show(StatusNotifierItem *self,
                                        gint32 menu_item_id);

GdkPixbuf *pixbuf_from_icon_data(GVariant *icon_data);
void *status_notifier_item_init(StatusNotifierItem *self, DbusItemV0Gen *proxy);
const gchar *status_notifier_item_get_category(StatusNotifierItem *self);
const gchar *status_notifier_item_get_id(StatusNotifierItem *self);
const gchar *status_notifier_item_get_title(StatusNotifierItem *self);
const gchar *status_notifier_item_get_status(StatusNotifierItem *self);
const int status_notifier_item_get_window_id(StatusNotifierItem *self);
const gchar *status_notifier_item_get_icon_name(StatusNotifierItem *self);
GdkPixbuf *status_notifier_item_get_icon_pixmap(StatusNotifierItem *self);
const gchar *status_notifier_item_get_overlay_icon_name(
    StatusNotifierItem *self);
GdkPixbuf *status_notifier_item_get_overlay_icon_pixmap(
    StatusNotifierItem *self);
const gchar *status_notifier_item_get_attention_icon_name(
    StatusNotifierItem *self);
GdkPixbuf *status_notifier_item_get_attention_icon_pixmap(
    StatusNotifierItem *self);
const gchar *status_notifier_item_get_attention_movie_name(
    StatusNotifierItem *self);
// TODO: get_tooltip
const gboolean status_notifier_item_get_item_is_menu(StatusNotifierItem *self);
const gchar *status_notifier_item_get_menu(StatusNotifierItem *self);
DbusItemV0Gen *status_notifier_item_get_proxy(StatusNotifierItem *self);
void status_notifier_item_free(StatusNotifierItem *self);

G_BEGIN_DECLS

// A Service which acts as a org.freedesktop.Notification daemon.
//
// Listens on DBUS for notifications and provides an API for acting upon
// them.
struct _StatusNotifierService;
#define NOTIFICATIONS_SERVICE_TYPE status_notifier_service_get_type()
G_DECLARE_FINAL_TYPE(StatusNotifierService, status_notifier_service,
                     STATUS_NOTIFIER, SERVICE, GObject);

G_END_DECLS

int status_notifier_service_global_init();

GHashTable *status_notifier_service_get_items(StatusNotifierService *self);

StatusNotifierService *status_notifier_service_get_global();
