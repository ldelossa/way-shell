#include "panel_clock.h"

#include <adwaita.h>

#include "../services/clock_service.h"
#include "../services/notifications_service/notifications_service.h"
#include "message_tray/message_tray.h"
#include "message_tray/message_tray_mediator.h"
#include "panel.h"

struct _PanelClock {
    GObject parent_instance;
    Panel *panel;
    GtkBox *container;
    GtkButton *button;
    GtkImage *notif;
    GtkLabel *label;
    gboolean toggled;
    gboolean dnd;
    // do-not-disturb setting is bound to dnd switch.
    GSettings *notifications_settings;
};
G_DEFINE_TYPE(PanelClock, panel_clock, G_TYPE_OBJECT)

static void panel_clock_on_notification_changed(NotificationsService *ns,
                                                GPtrArray *notifications,
                                                PanelClock *self) {
    if (self->dnd)
        return;

    if (notifications->len > 0) {
        gtk_widget_set_visible(GTK_WIDGET(self->notif), true);
    } else {
        gtk_widget_set_visible(GTK_WIDGET(self->notif), false);
    }
}

static void on_tick(ClockService *cs, GDateTime *now, PanelClock *self) {
    gchar *date = g_date_time_format(now, "%b %d %I:%M");
    gtk_label_set_text(self->label, date);
    g_free(date);
}

// stub out dispose, finalize, class_init, and init methods
static void panel_clock_dispose(GObject *gobject) {
    PanelClock *self = PANEL_CLOCK(gobject);

    // stop signal handler for tick
    ClockService *cs = clock_service_get_global();
    g_signal_handlers_disconnect_by_func(cs, G_CALLBACK(on_tick), self);

    // stop handler for notifications changed
    g_signal_handlers_disconnect_by_func(
        notifications_service_get_global(),
        G_CALLBACK(panel_clock_on_notification_changed), self);

    // Chain-up
    G_OBJECT_CLASS(panel_clock_parent_class)->dispose(gobject);
};

static void panel_clock_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(panel_clock_parent_class)->finalize(gobject);
};

static void panel_clock_class_init(PanelClockClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = panel_clock_dispose;
    object_class->finalize = panel_clock_finalize;
};

static void on_click(GtkWidget *w, PanelClock *clock) {
    MessageTrayMediator *m = message_tray_get_global_mediator();
    if (!m) return;
    if (clock->toggled) {
        message_tray_mediator_req_close(m);
        return;
    }
    message_tray_mediator_req_open(m, panel_get_monitor(clock->panel));
}

static void panel_clock_on_dnd_changed(GSettings *settings, gchar *key,
                                       PanelClock *self) {
    self->dnd = g_settings_get_boolean(settings, "do-not-disturb");
    if (self->dnd) {
        // set icon to notifications-disabled-symbolic
        gtk_image_set_from_icon_name(self->notif,
                                     "notifications-disabled-symbolic");
        // set visible
        gtk_widget_set_visible(GTK_WIDGET(self->notif), true);
    } else {
        // set icon back to notifications-symbolic
        gtk_image_set_from_icon_name(self->notif, "preferences-system-notifications-symbolic");
        // evaluate latest notifications state
        panel_clock_on_notification_changed(
            notifications_service_get_global(),
            notifications_service_get_notifications(
                notifications_service_get_global()),
            self);
    }
}

static void panel_clock_init_layout(PanelClock *self) {
    self->toggled = false;

    // set initial value before we wait for clock service
    GDateTime *now = g_date_time_new_now_local();
    gchar *date = g_date_time_format(now, "%b %d %I:%M");

    // setup signal to global clock service
    ClockService *cs = clock_service_get_global();
    g_signal_connect(cs, "tick", G_CALLBACK(on_tick), self);

    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->container), "panel-clock");

    self->button = GTK_BUTTON(gtk_button_new());
    gtk_widget_add_css_class(GTK_WIDGET(self->button), "panel-button");
    self->label = GTK_LABEL(gtk_label_new(date));
    gtk_button_set_child(self->button, GTK_WIDGET(self->label));
    gtk_box_append(self->container, GTK_WIDGET(self->button));

    self->notif = GTK_IMAGE(gtk_image_new_from_icon_name(
        "preferences-system-notifications-symbolic"));
    gtk_widget_add_css_class(GTK_WIDGET(self->notif), "panel-clock-notif");
    gtk_widget_set_visible(GTK_WIDGET(self->notif), false);
    gtk_box_append(self->container, GTK_WIDGET(self->notif));

    g_signal_connect(self->button, "clicked", G_CALLBACK(on_click), self);

    // get notifications service and wire up to notification-changed
    NotificationsService *ns = notifications_service_get_global();

    panel_clock_on_notification_changed(
        ns, notifications_service_get_notifications(ns), self);

    g_signal_connect(ns, "notification-changed",
                     G_CALLBACK(panel_clock_on_notification_changed), self);

    self->notifications_settings = g_settings_new("org.ldelossa.wlr-shell.notifications");
    // wire into dnd changed
    g_signal_connect(self->notifications_settings, "changed::do-not-disturb",
                     G_CALLBACK(panel_clock_on_dnd_changed), self);

}

void panel_clock_reinitialize(PanelClock *self) {
    // disconnect from signals
    ClockService *cs = clock_service_get_global();
    g_signal_handlers_disconnect_by_func(cs, G_CALLBACK(on_tick), self);
    g_signal_handlers_disconnect_by_func(
        notifications_service_get_global(),
        G_CALLBACK(panel_clock_on_notification_changed), self);
    // reset our layout
    panel_clock_init_layout(self);
}

static void panel_clock_init(PanelClock *self) {
    panel_clock_init_layout(self);
};

void panel_clock_set_panel(PanelClock *self, Panel *panel) {
    self->panel = panel;
}

void panel_clock_set_toggled(PanelClock *self, gboolean toggled) {
    if (toggled) {
        gtk_widget_add_css_class(GTK_WIDGET(self->button),
                                 "panel-button-toggled");
    } else {
        gtk_widget_remove_css_class(GTK_WIDGET(self->button),
                                    "panel-button-toggled");
    }
    self->toggled = toggled;
};

gboolean panel_clock_get_toggled(PanelClock *self) { return self->toggled; };

GtkWidget *panel_clock_get_widget(PanelClock *self) {
    return GTK_WIDGET(self->container);
};

GtkButton *panel_clock_get_button(PanelClock *self) { return self->button; };
