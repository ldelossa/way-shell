#include "./notification_osd.h"

#include <adwaita.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

#include "../message_tray.h"
#include "./notification_widget.h"
#include "gtk/gtkrevealer.h"
#include "notifications_list.h"

enum signals { signals_n };

typedef struct _NotificationsOSD {
    GObject parent_instance;
    NotificationsList *list;
    AdwWindow *win;
    GtkBox *container;
    NotificationWidget *notification;
    GtkRevealer *revealer;
    GtkEventControllerMotion *ctlr;
    gboolean message_tray_visible;
    guint32 timeout_id;
} NotificationsOSD;
static guint osd_signals[signals_n] = {0};
G_DEFINE_TYPE(NotificationsOSD, notifications_osd, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init and init methods.
static void notifications_osd_dispose(GObject *gobject) {
    NotificationsOSD *self = NOTIFICATIONS_OSD(gobject);

    // Chain-up
    G_OBJECT_CLASS(notifications_osd_parent_class)->dispose(gobject);
};

static void notifications_osd_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(notifications_osd_parent_class)->finalize(gobject);
};

static void notifications_osd_class_init(NotificationsOSDClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = notifications_osd_dispose;
    object_class->finalize = notifications_osd_finalize;
};

static void do_cleanup(NotificationsOSD *self) {
    // debug self->notification pointer value in hex
    g_debug("notification_osd.c:do_cleanup() self->notification: %p",
            self->notification);

    if (self->notification) {
        if (self->timeout_id) {
            g_source_remove(self->timeout_id);
            self->timeout_id = 0;
        }
        g_object_unref(self->notification);
        self->notification = NULL;
    }
}

// cleanup a notification once the revealer finishes the animation.
// this is split into two functions, where 'cleanup_osd' expects all the Gtk
// components to be alive and 'do_cleanup' can be ran if the Gtk components have
// been destroyed.
static void cleanup_osd(NotificationsOSD *self) {
    do_cleanup(self);
    gtk_box_remove(GTK_BOX(self->container),
                   gtk_widget_get_first_child(GTK_WIDGET(self->container)));
    gtk_widget_set_visible(GTK_WIDGET(self->win), false);
}

static void on_window_destroy(GtkWindow *win, NotificationsOSD *self) {
    g_debug("notification_osd.c:on_window_destroy() called");
    do_cleanup(self);
    notification_osd_reinitialize(self);
}

static void on_tray_will_show(MessageTray *tray, NotificationsOSD *self) {
    g_debug("notification_osd.c:on_tray_will_show() called");
    if (!self->win) return;
    gtk_revealer_set_reveal_child(self->revealer, false);
}

static void on_tray_visible(MessageTray *tray, NotificationsOSD *self) {
    g_debug("notification_osd.c:on_tray_visible() called");
    self->message_tray_visible = true;
}

static void on_tray_hidden(MessageTray *tray, NotificationsOSD *self) {
    g_debug("notification_osd.c:on_tray_hidden() called");
    self->message_tray_visible = false;
}

static void on_notifications_removed(NotificationsService *ns,
                                     GPtrArray *notifications, guint32 id,
                                     guint32 index, NotificationsOSD *self) {
    g_debug("notification_osd.c:on_notifications_removed() called");

    if (self->message_tray_visible) {
        return;
    }

    if (!self->notification) {
        return;
    }

    gtk_revealer_set_reveal_child(self->revealer, false);
}

static gboolean timed_dismiss(NotificationsOSD *self) {
    if (!self->win) return false;
    if (gtk_event_controller_motion_contains_pointer(self->ctlr)) {
        g_timeout_add_seconds(8, (GSourceFunc)timed_dismiss, self);
        return false;
    }
    gtk_revealer_set_reveal_child(self->revealer, false);
    return false;
}

static void on_child_revealed(GObject *object, GParamSpec *pspec,
                              NotificationsOSD *self) {
    if (!gtk_revealer_get_child_revealed(self->revealer))
        gtk_widget_set_visible(GTK_WIDGET(self->win), false);
}

void notification_osd_reinitialize(NotificationsOSD *self);

static void shrink(NotificationsOSD *self) {
    gtk_window_set_default_size(GTK_WINDOW(self->win), 440, 100);
}

static void on_notification_widget_collapsed(NotificationWidget *widget,
                                             NotificationsOSD *self) {
    g_debug("notification_osd.c:on_notification_widget_collapsed() called");
    shrink(self);
}

static void on_notification_added(NotificationsService *ns,
                                  GPtrArray *notifications, guint32 id,
                                  guint32 index, NotificationsOSD *self) {
    g_debug("notification_osd.c:on_notification_added() called");

    if (self->message_tray_visible || notifications_list_is_dnd(self->list)) {
        return;
    }

    cleanup_osd(self);
    gtk_revealer_set_reveal_child(self->revealer, false);

    Notification *n = g_ptr_array_index(notifications, index);
    if (!n) {
        return;
    }

    NotificationWidget *new = notification_widget_from_notification(n, true);
	notification_widget_set_osd(new, self);

    // for some reason, we need to reset this before presenting, despite
    // them being set in the constructing function.
    GtkLabel *summary = notification_widget_get_summary(new);
    GtkLabel *body = notification_widget_get_body(new);
    gtk_label_set_ellipsize(summary, PANGO_ELLIPSIZE_END);
    gtk_label_set_ellipsize(body, PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(summary, 30);
    gtk_label_set_max_width_chars(body, 30);

    g_signal_connect(new, "notification-collapsed",
                     G_CALLBACK(on_notification_widget_collapsed), self);

    gtk_box_append(self->container, notification_widget_get_widget(new));

    gtk_window_present(GTK_WINDOW(self->win));

    self->notification = new;

    gtk_revealer_set_reveal_child(self->revealer, true);

    // add a callback that runs in five seconds to dismiss the notification
    self->timeout_id =
        g_timeout_add_seconds(8, (GSourceFunc)timed_dismiss, self);
}

static void notifications_osd_init_layout(NotificationsOSD *self) {
    self->win = ADW_WINDOW(adw_window_new());
    gtk_widget_set_size_request(GTK_WIDGET(self->win), 440, 100);

    // wire into window destroy event
    g_signal_connect(self->win, "destroy", G_CALLBACK(on_window_destroy), self);

    // configure layershell, top layer and center
    gtk_layer_init_for_window(GTK_WINDOW(self->win));
    gtk_layer_set_namespace(GTK_WINDOW(self->win),
                            "way-shell-notifications-osd");
    gtk_layer_set_layer((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_widget_set_name(GTK_WIDGET(self->win), "notifications-osd");
    gtk_layer_set_anchor(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_TOP, true);
    gtk_layer_set_margin(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_TOP, 10);
    gtk_widget_set_visible(GTK_WIDGET(self->win), false);

    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->container),
                        "notifications-osd-container");

    self->revealer = GTK_REVEALER(gtk_revealer_new());
    gtk_revealer_set_transition_type(self->revealer,
                                     GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(self->revealer, 350);
    gtk_revealer_set_child(self->revealer, GTK_WIDGET(self->container));

    // wire into "notify::child-revealed"
    g_signal_connect(self->revealer, "notify::child-revealed",
                     G_CALLBACK(on_child_revealed), self);

    self->ctlr = GTK_EVENT_CONTROLLER_MOTION(gtk_event_controller_motion_new());
    gtk_widget_add_controller(GTK_WIDGET(self->container),
                              GTK_EVENT_CONTROLLER(self->ctlr));

    // listen for notification add events
    NotificationsService *ns = notifications_service_get_global();
    g_signal_connect(ns, "notification-added",
                     G_CALLBACK(on_notification_added), self);
    g_signal_connect(ns, "notification-closed",
                     G_CALLBACK(on_notifications_removed), self);

    MessageTray *mt = message_tray_get_global();
    g_signal_connect(mt, "message-tray-visible", G_CALLBACK(on_tray_visible),
                     self);
    g_signal_connect(mt, "message-tray-hidden", G_CALLBACK(on_tray_hidden),
                     self);
    g_signal_connect(mt, "message-tray-will-show",
                     G_CALLBACK(on_tray_will_show), self);

    adw_window_set_content(self->win, GTK_WIDGET(self->revealer));
}

void notification_osd_reinitialize(NotificationsOSD *self) {
    // destroy signals
    NotificationsService *ns = notifications_service_get_global();
    g_signal_handlers_disconnect_by_func(ns, on_notification_added, self);
    g_signal_handlers_disconnect_by_func(ns, on_notifications_removed, self);

    MessageTray *mt = message_tray_get_global();
    g_signal_handlers_disconnect_by_func(mt, on_tray_visible, self);
    g_signal_handlers_disconnect_by_func(mt, on_tray_hidden, self);
    g_signal_handlers_disconnect_by_func(mt, on_tray_will_show, self);

    notifications_osd_init_layout(self);
}

static void notifications_osd_init(NotificationsOSD *self) {
    notifications_osd_init_layout(self);
};

void notification_osd_set_notification_list(NotificationsOSD *self,
                                            NotificationsList *list) {
    self->list = list;
}

void notification_osd_hide(NotificationsOSD *self) {
    if (!self->win) return;
    gtk_revealer_set_reveal_child(self->revealer, false);

    // if timer, kill is
    if (self->timeout_id) {
        g_source_remove(self->timeout_id);
        self->timeout_id = 0;
    }
}
