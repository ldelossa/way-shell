#include "notification_group.h"

#include <adwaita.h>

#include "../../../services/notifications_service/notifications_service.h"
#include "../../message_tray/message_tray.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "notification_widget.h"

enum signals {
    notification_group_empty,
    notification_group_will_expand,
    notification_group_expanded,
    notification_group_collapsed,
    notification_added,
    notification_closed,
    notification_expanded,
    notification_collapsed,
    signals_n
};

// The NotificaionGroup is a complex widget which consists of the following
// layout at a high level:
//
// [    List Header         ]
// [    Head Notification   ]
// [    Notification List   ]
//
// Head Notification is always the latest notification seen for the App the
// group aggregates notifications for.
//
// List Header, an informational header which provides expansion and dismissal
// of the group, and Notification List, a list of older notifications aggergated
// by this group, are initially hidden in a slide up revealer and a slide down
// revealer, respectively.
//
// When the NotificationGroup is collapsed (not expanded) and contains more then
// 1 notification (the head notification) an overlay button which sits ontop of
// the entire Head Notification widget is visible. Clicking this acts as an
// expand button, which reveals the List Header and Notification List.
//
// The visual stacking effect is aided by the NotificationWidget GObject itself,
// which can be configured to display a "stacking effect" on request, this is
// not implemented by the NotificationGroup.
typedef struct _NotificationGroup {
    GObject parent_instance;
    GtkBox *container;

    // notification list header
    GtkCenterBox *list_header;
    GtkLabel *app_name;
    GtkButton *conceal_button;
    GtkButton *dismiss_button;

    // notification list
    GtkBox *notification_list;

    // head container box contains:
    // top revealer - notification list header
    // notification widget
    // bottom revealer - notification list
    GtkBox *head_notification_container;
    GtkRevealer *notification_list_header_revealer;
    GtkRevealer *notification_list_revealer;
    NotificationWidget *head_notification;

    // Create an overlay button which sits ontop of the head notification, when
    // clicked it will expand the head notification into the notification list.
    GtkButton *overlay_expand_button;
    GtkOverlay *overlay;

    // index 0 always displayed as overlay's overlay widget.
    GHashTable *notification_widgets;

    // properties
    gchar *app;
    gboolean expanded;
} NotificationGroup;

static guint notification_group_signals[signals_n] = {0};
G_DEFINE_TYPE(NotificationGroup, notification_group, G_TYPE_OBJECT);

void on_notification_added(NotificationsService *service,
                           GPtrArray *notifications, guint32 id, guint32 index,
                           NotificationGroup *self);

void on_notification_closed(NotificationsService *service,
                            GPtrArray *notifications, guint32 id, guint32 index,
                            NotificationGroup *self);

static void on_message_tray_will_hide(MessageTray *m, NotificationGroup *self);

// stub out dispose, finalize, class_init and init methods.
static void notification_group_dispose(GObject *object) {
    G_OBJECT_CLASS(notification_group_parent_class)->dispose(object);

    NotificationGroup *self = NOTIFICATION_GROUP(object);

    g_hash_table_remove_all(self->notification_widgets);

    // kill signals
    NotificationsService *ns = notifications_service_get_global();
    g_signal_handlers_disconnect_by_func(ns, on_notification_added, object);
    g_signal_handlers_disconnect_by_func(ns, on_notification_closed, object);

    MessageTray *mt = message_tray_get_global();
    g_signal_handlers_disconnect_by_func(mt, on_message_tray_will_hide, object);
}

static void notification_group_finalize(GObject *object) {
    G_OBJECT_CLASS(notification_group_parent_class)->finalize(object);
}

static void notification_group_class_init(NotificationGroupClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = notification_group_dispose;
    object_class->finalize = notification_group_finalize;

    notification_group_signals[notification_group_empty] =
        g_signal_new("notification-group-empty", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    notification_group_signals[notification_group_will_expand] =
        g_signal_new("notification-group-will-expand", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    notification_group_signals[notification_group_expanded] =
        g_signal_new("notification-group-expanded", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    notification_group_signals[notification_group_collapsed] =
        g_signal_new("notification-group-collapsed", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    notification_group_signals[notification_added] = g_signal_new(
        "notification-group-notification-added", G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    notification_group_signals[notification_closed] = g_signal_new(
        "notification-group-notification-closed", G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    notification_group_signals[notification_expanded] = g_signal_new(
        "notification-group-notification-expanded", G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    notification_group_signals[notification_collapsed] = g_signal_new(
        "notification-group-notification-collapsed", G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

// configures the NotificaionGroup widget appropriately given the state of
// expansion and the number of notifications the group has registered.
static void apply_expansion_rules(NotificationGroup *self) {
    guint size = g_hash_table_size(self->notification_widgets);

    if (size == 1) {
        // close revealers
        gtk_revealer_set_reveal_child(self->notification_list_header_revealer,
                                      false);
        gtk_revealer_set_reveal_child(self->notification_list_revealer, false);
        // remove stack effect from head widget
        notification_widget_set_stack_effect(self->head_notification, false);
        // remove overlay expand button
        gtk_widget_set_visible(GTK_WIDGET(self->overlay_expand_button), false);
        // force expansion to close.
        self->expanded = false;
        g_signal_emit(
            self, notification_group_signals[notification_group_collapsed], 0);
    }
    if (size > 1 && self->expanded) {
        // show header revelear
        gtk_revealer_set_reveal_child(self->notification_list_header_revealer,
                                      true);
        // show notification list revealer
        gtk_revealer_set_reveal_child(self->notification_list_revealer, true);
        // remove stack effect from head widget
        notification_widget_set_stack_effect(self->head_notification, false);
        // remove overlay expand button
        gtk_widget_set_visible(GTK_WIDGET(self->overlay_expand_button), false);
    }
    if (size > 1 && !self->expanded) {
        // hide header revelear
        gtk_revealer_set_reveal_child(self->notification_list_header_revealer,
                                      false);
        // hide notification list revealer
        gtk_revealer_set_reveal_child(self->notification_list_revealer, false);
        // set stack effect on head widget
        notification_widget_set_stack_effect(self->head_notification, true);
        // show overlay expand button
        gtk_widget_set_visible(GTK_WIDGET(self->overlay_expand_button), true);
    }
}

static void expand_messages_on_click(GtkButton *button,
                                     NotificationGroup *self) {
    g_debug("notification_group.c:on_overlay_expander_button_clicked called");

    if (self->expanded) {
        self->expanded = !self->expanded;
        apply_expansion_rules(self);
    } else {
        self->expanded = !self->expanded;
        g_signal_emit(
            self, notification_group_signals[notification_group_will_expand],
            0);
        apply_expansion_rules(self);
    }
}

static void on_dismiss_button_clicked(GtkButton *button,
                                      NotificationGroup *self) {
    g_debug("notification_group.c:on_dismiss_button_clicked called");
    notification_group_dismiss_all(self);
    // notification chains will do everything else we need to cleanup out
    // group, nothing further needed here.
}

static void on_notification_list_revealed(GtkRevealer *revealer,
                                          GParamSpec *pspec,
                                          NotificationGroup *self) {
    if (gtk_revealer_get_child_revealed(revealer)) {
        g_signal_emit(
            self, notification_group_signals[notification_group_expanded], 0);
    } else {
        g_signal_emit(
            self, notification_group_signals[notification_group_collapsed], 0);
    }
}

static void on_message_tray_will_hide(MessageTray *tray,
                                      NotificationGroup *self) {
    g_debug("notification_group.c:on_message_tray_will_hide called");
    if (self->expanded) {
        self->expanded = !self->expanded;
        apply_expansion_rules(self);
    }
}

static void notification_group_init_layout(NotificationGroup *self) {
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));

    // create notification list header
    self->list_header = GTK_CENTER_BOX(gtk_center_box_new());
    gtk_widget_add_css_class(GTK_WIDGET(self->list_header),
                             "notification-group-list-header");
    self->app_name = GTK_LABEL(gtk_label_new(""));
    gtk_widget_add_css_class(GTK_WIDGET(self->app_name),
                             "notification-group-app-name");
    self->conceal_button =
        GTK_BUTTON(gtk_button_new_from_icon_name("view-restore-symbolic"));
    gtk_widget_add_css_class(GTK_WIDGET(self->conceal_button), "circular");
    gtk_widget_add_css_class(GTK_WIDGET(self->conceal_button),
                             "notification-group-conceal-button");
    g_signal_connect(self->conceal_button, "clicked",
                     G_CALLBACK(expand_messages_on_click), self);

    self->dismiss_button =
        GTK_BUTTON(gtk_button_new_from_icon_name("window-close-symbolic"));
    gtk_widget_add_css_class(GTK_WIDGET(self->dismiss_button), "circular");
    gtk_widget_add_css_class(GTK_WIDGET(self->dismiss_button),
                             "notification-group-dismiss-button");
    gtk_center_box_set_start_widget(self->list_header,
                                    GTK_WIDGET(self->app_name));
    g_signal_connect(self->dismiss_button, "clicked",
                     G_CALLBACK(on_dismiss_button_clicked), self);

    GtkBox *list_header_buttons =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_box_append(list_header_buttons, GTK_WIDGET(self->conceal_button));
    gtk_box_append(list_header_buttons, GTK_WIDGET(self->dismiss_button));

    gtk_center_box_set_end_widget(self->list_header,
                                  GTK_WIDGET(list_header_buttons));

    // create header revelear
    self->notification_list_header_revealer = GTK_REVEALER(gtk_revealer_new());
    gtk_revealer_set_transition_type(self->notification_list_header_revealer,
                                     GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
    gtk_revealer_set_child(self->notification_list_header_revealer,
                           GTK_WIDGET(self->list_header));

    g_signal_connect(self->notification_list_header_revealer,
                     "notify::child-revealed",
                     G_CALLBACK(on_notification_list_revealed), self);

    // create head notification container
    self->head_notification_container =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->head_notification_container),
                             "notification-group-head-notification-container");

    // create notification list revealer
    self->notification_list = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->notification_list),
                             "notification-group-notification-list");

    self->notification_list_revealer = GTK_REVEALER(gtk_revealer_new());
    gtk_revealer_set_transition_type(self->notification_list_revealer,
                                     GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_child(self->notification_list_revealer,
                           GTK_WIDGET(self->notification_list));

    // create overlay expand button
    self->overlay = GTK_OVERLAY(gtk_overlay_new());
    self->overlay_expand_button = GTK_BUTTON(gtk_button_new());
    gtk_widget_add_css_class(GTK_WIDGET(self->overlay_expand_button),
                             "notification-group-overlay-expand-button");
    gtk_widget_set_visible(GTK_WIDGET(self->overlay_expand_button), false);
    g_signal_connect(self->overlay_expand_button, "clicked",
                     G_CALLBACK(expand_messages_on_click), self);

    gtk_overlay_add_overlay(self->overlay,
                            GTK_WIDGET(self->overlay_expand_button));
    gtk_overlay_set_child(self->overlay,
                          GTK_WIDGET(self->head_notification_container));

    // wire into message_tray_hidden event
    MessageTray *mt = message_tray_get_global();
    g_signal_connect(mt, "message-tray-will-hide",
                     G_CALLBACK(on_message_tray_will_hide), self);

    // add overlay to main container
    gtk_box_append(self->container, GTK_WIDGET(self->overlay));
}

// moves the current head notification to the top of the expandable notification
// list and sets `n` as the new head notification.
static void push_new_head(NotificationGroup *self,
                          NotificationWidget *new_head) {
    NotificationWidget *head = self->head_notification;
    notification_widget_set_stack_effect(head, false);

    // remove middle widget (head notification widget), prepend new one, then
    // reorder header back to front, leaving [header, head_notification,
    // notification_list] order.
    //
    // immediately ref head, since the following removal decs the ref count.
    g_object_ref(notification_widget_get_widget(head));
    gtk_box_remove(self->head_notification_container,
                   notification_widget_get_widget(head));
    gtk_box_prepend(self->head_notification_container,
                    notification_widget_get_widget(new_head));
    gtk_box_reorder_child_after(
        self->head_notification_container,
        GTK_WIDGET(self->notification_list_header_revealer), NULL);

    // prepend current head to notification list
    gtk_box_prepend(self->notification_list,
                    notification_widget_get_widget(head));

    // sway head pointers
    self->head_notification = new_head;
}

static void clear_head(NotificationGroup *self) {
    // remove from hashtable, this also unrefs self->head_notification, don't
    // double unref.
    g_hash_table_remove(
        self->notification_widgets,
        GUINT_TO_POINTER(notification_widget_get_id(self->head_notification)));

    // remove the GtkWidget from its place in head_notification_container
    gtk_box_remove(self->head_notification_container,
                   notification_widget_get_widget(self->head_notification));

    self->head_notification = NULL;
}

// Pops the top most widget in notifications_list into the notification head.
// self->head_notification and its associated widget should be cleared before
// calling this.
static void pop_to_head(NotificationGroup *self) {
    GtkWidget *top =
        gtk_widget_get_first_child(GTK_WIDGET(self->notification_list));
    if (!top) return;

    guint32 notification_id =
        GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(top), "notification-id"));
    NotificationWidget *w = g_hash_table_lookup(
        self->notification_widgets, GUINT_TO_POINTER(notification_id));
    if (!w) return;

    g_object_ref(top);
    gtk_box_remove(self->notification_list, top);

    self->head_notification = w;

    gtk_box_prepend(self->head_notification_container,
                    notification_widget_get_widget(w));
    gtk_box_reorder_child_after(
        self->head_notification_container,
        GTK_WIDGET(self->notification_list_header_revealer), NULL);
}

static void notification_group_init(NotificationGroup *self) {
    self->notification_widgets = g_hash_table_new_full(
        g_direct_hash, g_direct_equal, NULL, g_object_unref);
    notification_group_init_layout(self);
}

static void on_notification_expand(NotificationWidget *w,
                                   NotificationGroup *self) {
    g_debug("notification_group.c:on_notification_expand called");
    g_signal_emit(self, notification_group_signals[notification_expanded], 0);
}

static void on_notification_collapse(NotificationWidget *w,
                                     NotificationGroup *self) {
    g_debug("notification_group.c:on_notification_collapse called");
    g_signal_emit(self, notification_group_signals[notification_collapsed], 0);
}

void on_notification_added(NotificationsService *service,
                           GPtrArray *notifications, guint32 id, guint32 index,
                           NotificationGroup *self) {
    g_debug("notification_group.c:on_notification_added called");

    Notification *n = g_ptr_array_index(notifications, index);
    if (g_strcmp0(n->app_name, self->app) != 0) return;

    NotificationWidget *new_head =
        notification_widget_from_notification(n, false);

    g_signal_connect(new_head, "notification-expanded",
                     G_CALLBACK(on_notification_expand), self);
    g_signal_connect(new_head, "notification-collapsed",
                     G_CALLBACK(on_notification_collapse), self);

    g_hash_table_insert(self->notification_widgets, GUINT_TO_POINTER(n->id),
                        new_head);

    push_new_head(self, new_head);

    g_signal_emit(self, notification_group_signals[notification_added], 0);

    apply_expansion_rules(self);
}

void on_notification_closed(NotificationsService *service,
                            GPtrArray *notifications, guint32 id, guint32 index,
                            NotificationGroup *self) {
    // if we don't have a widget created for the notification closing,
    // just ignore, another group may handle this.
    NotificationWidget *w =
        g_hash_table_lookup(self->notification_widgets, GUINT_TO_POINTER(id));
    if (!w) return;

    // check if id is our head notification
    if (notification_widget_get_id(self->head_notification) == id) {
        // we are the only notification, remove ourselves and inform parent
        // we are empty.
        if (g_hash_table_size(self->notification_widgets) == 1) {
            g_signal_emit(
                self, notification_group_signals[notification_group_empty], 0);
            return;
        }

        // we are not the only notification, pop top of notification list into
        // head.
        clear_head(self);
        pop_to_head(self);

        if (self->expanded) {
            notification_widget_set_stack_effect(self->head_notification,
                                                 false);
        } else {
            gtk_widget_set_visible(GTK_WIDGET(self->overlay_expand_button),
                                   true);
            notification_widget_set_stack_effect(self->head_notification, true);
        }

        g_signal_emit(self, notification_group_signals[notification_closed], 0);

        apply_expansion_rules(self);
        return;
    }

    // removed notification is not our head notification, we can remove it
    // from our list
    gtk_box_remove(self->notification_list, notification_widget_get_widget(w));

    // this also unrefs notification.id == id, don't double unref.
    g_hash_table_remove(self->notification_widgets, GUINT_TO_POINTER(id));

    g_signal_emit(self, notification_group_signals[notification_closed], 0);

    apply_expansion_rules(self);
}

void notification_group_add_notification(NotificationGroup *self,
                                         Notification *n) {
    // set app name
    self->app = g_strdup(n->app_name);

    // set header's app name.
    gtk_label_set_text(self->app_name, n->app_name);

    // create notification widget
    NotificationWidget *new_head =
        notification_widget_from_notification(n, false);

    g_signal_connect(new_head, "notification-expanded",
                     G_CALLBACK(on_notification_expand), self);
    g_signal_connect(new_head, "notification-collapsed",
                     G_CALLBACK(on_notification_collapse), self);

    self->head_notification = new_head;

    g_hash_table_insert(self->notification_widgets, GUINT_TO_POINTER(n->id),
                        new_head);

    // setup head notification container
    gtk_box_append(self->head_notification_container,
                   GTK_WIDGET(self->notification_list_header_revealer));
    gtk_box_append(self->head_notification_container,
                   notification_widget_get_widget(new_head));
    gtk_box_append(self->head_notification_container,
                   GTK_WIDGET(self->notification_list_revealer));

    // watch notification for added and removed notifications for our app
    NotificationsService *ns = notifications_service_get_global();

    // seed existing notifications
    GPtrArray *notifications = notifications_service_get_notifications(ns);
    for (guint32 i = 0; i < notifications->len; i++) {
        Notification *nn = g_ptr_array_index(notifications, i);

        // don't seed the notification we just added.
        if (nn->id == n->id) {
            continue;
        }

        // ignore notifications not for us
        if (g_strcmp0(nn->app_name, self->app) != 0) {
            continue;
        }

        // seed it.
        if (g_strcmp0(nn->app_name, self->app) == 0) {
            on_notification_added(ns, notifications, nn->id, i, self);
        }
    }
    apply_expansion_rules(self);

    g_signal_connect(ns, "notification-added",
                     G_CALLBACK(on_notification_added), self);
    g_signal_connect(ns, "notification-closed",
                     G_CALLBACK(on_notification_closed), self);
}

GtkWidget *notification_group_get_widget(NotificationGroup *self) {
    return GTK_WIDGET(self->container);
}

gchar *notification_group_get_app_name(NotificationGroup *self) {
    return self->app;
}

void notification_group_dismiss_all(NotificationGroup *self) {
    GList *notification_widgets =
        g_hash_table_get_values(self->notification_widgets);

    for (GList *l = notification_widgets; l; l = l->next) {
        NotificationWidget *w = l->data;
        notification_widget_dismiss_notification(w);
        // this group handles the NotificationService.notification-closed events
        // created the above method call and will eventually emit an empty
        // signal once all its notifications are closed.
    }
}
