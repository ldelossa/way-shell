#include "notifications_list.h"

#include <adwaita.h>

#include "../../../services/notifications_service/notifications_service.h"
#include "../message_tray.h"
#include "./notification_group.h"
#include "./notification_osd.h"
#include "./notification_widget.h"

enum signals { signals_n };

struct _NotificationsList {
    GObject parent_instance;
    MessageTray *tray;
    GtkBox *container;
    GtkBox *list_container;
    GtkScrolledWindow *scroll;
    GtkBox *list;
    GtkCenterBox *controls;
    AdwSwitchRow *dnd_switch;
    AdwStatusPage *status;
    GtkButton *clear;
    NotificationsOSD *osd;
    GSettings *settings;
    GHashTable *notification_groups;
};
static guint signals[signals_n] = {0};
G_DEFINE_TYPE(NotificationsList, notifications_list, G_TYPE_OBJECT);

static void on_notification_group_empty(NotificationGroup *group,
                                        NotificationsList *self) {
    g_debug("notifications_list.c:notification_group_empty() called");
    gtk_box_remove(self->list, notification_group_get_widget(group));
    g_hash_table_remove(self->notification_groups,
                        notification_group_get_app_name(group));
}

void on_notifications_added(NotificationsService *service,
                            GPtrArray *notifications, guint32 id, guint32 index,
                            NotificationsList *self) {
    g_debug(
        "notifications_list.c:on_notifications_changed() notifications "
        "changed");

    // lookup notification in array
    Notification *n = g_ptr_array_index(notifications, index);
    if (!n) {
        g_warning(
            "notifications_list.c:on_notifications_changed() notification "
            "not found in array");
        return;
    }

    // swap the status page if we need to.
    if (notifications->len > 0) {
        gtk_widget_set_visible(GTK_WIDGET(self->status), false);
        gtk_widget_set_visible(GTK_WIDGET(self->scroll), true);
    } else {
        gtk_widget_set_visible(GTK_WIDGET(self->status), true);
        gtk_widget_set_visible(GTK_WIDGET(self->scroll), false);
    }

    if (!g_hash_table_contains(self->notification_groups, n->app_name)) {
        NotificationGroup *nw = g_object_new(NOTIFICATION_GROUP_TYPE, NULL);
        notification_group_add_notification(nw, n);
        gtk_box_prepend(self->list,
                        GTK_WIDGET(notification_group_get_widget(nw)));
        g_hash_table_insert(self->notification_groups, g_strdup(n->app_name),
                            nw);
        g_signal_connect(nw, "notification-group-empty",
                         G_CALLBACK(on_notification_group_empty), self);
    }
}

// stub out dispose, finalize, class_init and init methods.
static void notifications_list_dispose(GObject *gobject) {
    NotificationsList *self = NOTIFICATIONS_LIST(gobject);

    // cancel signals
    NotificationsService *service = notifications_service_get_global();
    g_signal_handlers_disconnect_by_func(service, on_notifications_added, self);

    // unref osd
    g_object_unref(self->osd);

    // Chain-up
    G_OBJECT_CLASS(notifications_list_parent_class)->dispose(gobject);
};

static void notifications_list_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(notifications_list_parent_class)->finalize(gobject);
};

static void notifications_list_class_init(NotificationsListClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = notifications_list_dispose;
    object_class->finalize = notifications_list_finalize;
};

static void on_clear_all_clicked(GtkButton *button, NotificationsList *self) {
    g_debug("notifications_list.c:on_clear_all_clicked() called");

    GList *notification_groups =
        g_hash_table_get_values(self->notification_groups);

    for (GList *l = notification_groups; l; l = l->next) {
        NotificationGroup *group = l->data;
        notification_group_dismiss_all(group);
        // each group will fire an empty signal, so we'll unref them at
        // that signal handler 'on_notification_group_empty'.
    }
}

static void notifications_list_init_layout(NotificationsList *self) {
    // container
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->container), "notifications-list");

    // scolled window containing notification list
    self->scroll = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
    gtk_widget_set_vexpand(GTK_WIDGET(self->scroll), true);
    gtk_scrolled_window_set_policy(self->scroll, GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);

    // status page displayed when no notifications are available
    self->status = ADW_STATUS_PAGE(adw_status_page_new());
    adw_status_page_set_icon_name(self->status,
                                  "notifications-disabled-symbolic");
    adw_status_page_set_title(self->status, "No Notifications");
    gtk_widget_set_vexpand(GTK_WIDGET(self->status), true);

    // container box for notification list
    self->list_container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->list_container),
                        "notifications-list-container");

    // box acting as lit of notification widgets
    self->list = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->list), "notifications-list-list");
    gtk_widget_set_vexpand(GTK_WIDGET(self->list), true);

    // center box which holds the dnd and clear all boxes
    self->controls = GTK_CENTER_BOX(gtk_center_box_new());
    gtk_widget_set_name(GTK_WIDGET(self->controls),
                        "notifications-list-controls");

    // dnd button
    self->dnd_switch = ADW_SWITCH_ROW(adw_switch_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->dnd_switch),
                                  "Do Not Disturb");
    gtk_widget_set_size_request(GTK_WIDGET(self->dnd_switch), 200, -1);
    gtk_widget_add_css_class(GTK_WIDGET(self->dnd_switch),
                             "notifications-list-dnd");

    // clear button
    self->clear = GTK_BUTTON(gtk_button_new_with_label("Clear"));
    gtk_widget_add_css_class(GTK_WIDGET(self->clear),
                             "notifications-list-clear");
    g_signal_connect(self->clear, "clicked", G_CALLBACK(on_clear_all_clicked),
                     self);

    // wire it up

    // dnd start widget of controls center box
    gtk_center_box_set_start_widget(self->controls,
                                    GTK_WIDGET(self->dnd_switch));
    // clear all button end widget of center box
    gtk_center_box_set_end_widget(self->controls, GTK_WIDGET(self->clear));

    // list-container child of container
    gtk_box_append(self->container, GTK_WIDGET(self->list_container));
    // status page child of list container
    gtk_box_append(self->list_container, GTK_WIDGET(self->status));
    // list child of scroll
    gtk_scrolled_window_set_child(self->scroll, GTK_WIDGET(self->list));
    // scroll child of list container
    gtk_box_append(self->list_container, GTK_WIDGET(self->scroll));
    // controls child of list_container
    gtk_box_append(self->list_container, GTK_WIDGET(self->controls));

    // start with scrolled widget hidden
    gtk_widget_set_visible(GTK_WIDGET(self->scroll), false);

    // seed the initial list of notifications
    NotificationsService *service = notifications_service_get_global();
    GPtrArray *notifications = notifications_service_get_notifications(service);
    for (int i = 0; i < notifications->len; i++) {
        Notification *n = g_ptr_array_index(notifications, i);
        on_notifications_added(service, notifications, n->id, i, self);
    }

    // getting gsettings
    self->settings = g_settings_new("org.ldelossa.way-shell.notifications");

    // bind do-not-distrub gsetting to switch's active's property
    g_settings_bind(self->settings, "do-not-disturb", self->dnd_switch,
                    "active", G_SETTINGS_BIND_DEFAULT);

    // connect to signals
    g_signal_connect(service, "notification-added",
                     G_CALLBACK(on_notifications_added), self);
}

void notifications_list_reinitialize(NotificationsList *self) {
    g_debug("notifications_list.c:notifications_list_reinitialize() called");

    // kill our signals
    NotificationsService *service = notifications_service_get_global();
    g_signal_handlers_disconnect_by_func(service, on_notifications_added, self);

    // remove all NotificationGroups, unrefing each one.
    g_hash_table_remove_all(self->notification_groups);

    // init our layout again
    notifications_list_init_layout(self);
}

static void notifications_list_init(NotificationsList *self) {
    self->notification_groups =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);

    self->osd = g_object_new(NOTIFICATIONS_OSD_TYPE, NULL);
    notification_osd_set_notification_list(self->osd, self);

    notifications_list_init_layout(self);
};

gboolean notifications_list_is_dnd(NotificationsList *self) {
    return adw_switch_row_get_active(ADW_SWITCH_ROW(self->dnd_switch));
}

GtkWidget *notifications_list_get_widget(NotificationsList *self) {
    return GTK_WIDGET(self->container);
}
