#include "notification_widget.h"

#include <adwaita.h>

#include "gtk/gtkrevealer.h"

static void notification_widget_init_layout(NotificationWidget *self,
                                            Notification *n) {
    // main container for widget
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->container),
                             "notification-widget");

    // motion controller to get mouse in/out events
    self->ctrl = GTK_EVENT_CONTROLLER_MOTION(gtk_event_controller_motion_new());
    gtk_widget_add_controller(GTK_WIDGET(self->container),
                              GTK_EVENT_CONTROLLER(self->ctrl));

    // create overlay to stack dismiss button ontop of overlay
    self->overlay = GTK_OVERLAY(gtk_overlay_new());

    // create main notification button.
    // it's child is a complex box layout
    // [[icon][text]] where text is a vertical box containing summary and body
    // labels.
    GtkBox *button_contents =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    self->button = GTK_BUTTON(gtk_button_new());
    gtk_button_set_child(self->button, GTK_WIDGET(button_contents));

    if (n->urgency == 2) {
        gtk_widget_add_css_class(GTK_WIDGET(self->button),
                                 "notification-widget-button-critical");
    } else {
        gtk_widget_add_css_class(GTK_WIDGET(self->button),
                                 "notification-widget-button");
    }

    // dismiss button used in overlay stack.
    GtkImage *dismiss_icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("window-close-symbolic"));
    gtk_image_set_pixel_size(dismiss_icon, 16);
    self->dismiss = GTK_BUTTON(gtk_button_new());
    gtk_widget_set_size_request(GTK_WIDGET(self->dismiss), 16, 16);
    gtk_button_set_child(self->dismiss, GTK_WIDGET(dismiss_icon));
    gtk_widget_set_halign(GTK_WIDGET(self->dismiss), GTK_ALIGN_END);
    gtk_widget_set_valign(GTK_WIDGET(self->dismiss), GTK_ALIGN_START);
    gtk_widget_add_css_class(GTK_WIDGET(self->dismiss), "circular");
    gtk_widget_add_css_class(GTK_WIDGET(self->dismiss),
                             "notification-widget-dismiss-button");

    // dismiss button gets a revealer to animate its appearence
    self->dismiss_revealer = GTK_REVEALER(gtk_revealer_new());
    gtk_revealer_set_transition_type(self->dismiss_revealer,
                                     GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
    gtk_widget_set_halign(GTK_WIDGET(self->dismiss_revealer), GTK_ALIGN_END);
    gtk_widget_set_valign(GTK_WIDGET(self->dismiss_revealer), GTK_ALIGN_START);
    gtk_revealer_set_transition_duration(self->dismiss_revealer, 250);
    gtk_revealer_set_reveal_child(self->dismiss_revealer, false);
    gtk_revealer_set_child(self->dismiss_revealer, GTK_WIDGET(self->dismiss));

    // setup overlay and add it to our container
    gtk_overlay_set_child(self->overlay, GTK_WIDGET(self->button));
    gtk_overlay_add_overlay(self->overlay, GTK_WIDGET(self->dismiss_revealer));
    gtk_box_append(self->container, GTK_WIDGET(self->overlay));
}

static void set_notification_icon(NotificationWidget *self, Notification *n) {
    GtkImage *icon = NULL;
    if (n->img_data.data) {
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data(
            (const guchar *)n->img_data.data,
            n->img_data.has_alpha ? GDK_COLORSPACE_RGB : GDK_COLORSPACE_RGB,
            n->img_data.has_alpha, n->img_data.bits_per_sample,
            n->img_data.width, n->img_data.height, n->img_data.rowstride, NULL,
            NULL);
        if (pixbuf) {
            // scale image to 48 pixels
            GdkPixbuf *scaled_pixbuf =
                gdk_pixbuf_scale_simple(pixbuf, 48, 48, GDK_INTERP_BILINEAR);

            GdkTexture *texture = gdk_texture_new_for_pixbuf(scaled_pixbuf);
            icon =
                GTK_IMAGE(gtk_image_new_from_paintable(GDK_PAINTABLE(texture)));
            g_object_unref(texture);
        }
    } else
        icon = GTK_IMAGE(gtk_image_new_from_icon_name(
            "preferences-system-notifications-symbolic"));

    gtk_image_set_pixel_size(icon, 48);
    gtk_widget_set_halign(GTK_WIDGET(icon), GTK_ALIGN_START);
    gtk_widget_add_css_class(GTK_WIDGET(icon), "notification-widget-icon");

    GtkBox *button_contents = GTK_BOX(gtk_button_get_child(self->button));
    gtk_box_append(button_contents, GTK_WIDGET(icon));
}

static void set_notification_text(NotificationWidget *self, Notification *n) {
    GtkBox *text = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));

    // make summary
    char *summary_text = g_strstrip(n->summary);
    summary_text = g_strdelimit(summary_text, "\n", ' ');
    GtkLabel *summary = GTK_LABEL(gtk_label_new(summary_text));
    gtk_widget_add_css_class(GTK_WIDGET(summary), "summary");
    gtk_widget_set_halign(GTK_WIDGET(summary), GTK_ALIGN_START);
    gtk_label_set_max_width_chars(summary, 40);
    gtk_label_set_ellipsize(summary, PANGO_ELLIPSIZE_END);
    gtk_label_set_lines(summary, 1);
    gtk_label_set_wrap(summary, true);
    self->summary = summary;

    // make body
    char *body_text = g_strstrip(n->body);
    body_text = g_strdelimit(body_text, "\n", ' ');
    GtkLabel *body = GTK_LABEL(gtk_label_new(body_text));
    gtk_widget_add_css_class(GTK_WIDGET(body), "body");
    gtk_widget_set_halign(GTK_WIDGET(body), GTK_ALIGN_START);
    gtk_label_set_max_width_chars(body, 200);
    gtk_label_set_ellipsize(body, PANGO_ELLIPSIZE_END);
    gtk_label_set_lines(body, 3);
    gtk_label_set_wrap(body, true);
    self->body = body;

    // add summary and body to text box.
    gtk_box_append(text, GTK_WIDGET(summary));
    gtk_box_append(text, GTK_WIDGET(body));
    GtkBox *button_contents = GTK_BOX(gtk_button_get_child(self->button));
    gtk_box_append(GTK_BOX(button_contents), GTK_WIDGET(text));
}

void on_pointer_enter(GtkEventControllerMotion *ctrl, double x, double y,
                      NotificationWidget *self) {
    gtk_revealer_set_reveal_child(self->dismiss_revealer, true);
}

void on_pointer_leave(GtkEventControllerMotion *ctrl, double x, double y,
                      NotificationWidget *self) {
    gtk_revealer_set_reveal_child(self->dismiss_revealer, false);
}

void on_dismiss_clicked(GtkButton *button, NotificationWidget *self) {
    g_debug("notification_widget.c:on_dismiss_clicked() called");

    NotificationsService *service = notifications_service_get_global();
    notifications_service_closed_notification(
        service, self->id, NOTIFICATIONS_CLOSED_REASON_DISMISSED);
}

void on_notification_clicked(GtkButton *button, NotificationWidget *self) {
    g_debug("notification_widget.c:on_notification_clicked() called");

    NotificationsService *service = notifications_service_get_global();
    notifications_service_invoke_action(service, self->id, "default");
    // dimiss it after click
    notifications_service_closed_notification(
        service, self->id, NOTIFICATIONS_CLOSED_REASON_REQUESTED);
}

NotificationWidget *notification_widget_from_notification(Notification *n) {
    NotificationWidget *self = g_malloc0(sizeof(NotificationWidget));

    self->id = n->id;

    notification_widget_init_layout(self, n);
    set_notification_icon(self, n);
    set_notification_text(self, n);

    // wire up notification click
    g_signal_connect(self->button, "clicked",
                     G_CALLBACK(on_notification_clicked), self);

    // wire up dissmiss click
    g_signal_connect(self->dismiss, "clicked", G_CALLBACK(on_dismiss_clicked),
                     self);

    // wire up motion controller
    g_signal_connect(self->ctrl, "enter", G_CALLBACK(on_pointer_enter), self);
    g_signal_connect(self->ctrl, "leave", G_CALLBACK(on_pointer_leave), self);

    // give the main container a pointer to ourselves.
    g_object_set_data(G_OBJECT(self->container), "self", self);

    return self;
}

void notification_widget_free(NotificationWidget *w) {
    g_debug("notification_widget.c:notification_widget_free() called");

    // remove signal handlers
    g_signal_handlers_disconnect_by_func(w->button, on_notification_clicked, w);
    g_signal_handlers_disconnect_by_func(w->dismiss, on_dismiss_clicked, w);
    g_signal_handlers_disconnect_by_func(w->ctrl, on_pointer_enter, w);
    g_signal_handlers_disconnect_by_func(w->ctrl, on_pointer_leave, w);

    g_free(w);
}