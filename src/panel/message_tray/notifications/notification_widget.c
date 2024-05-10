#include "notification_widget.h"

#include <adwaita.h>
#include <string.h>

#include "../../../services/media_player_service/media_player_service.h"
#include "../message_tray.h"
#include "glib.h"
#include "gtk/gtk.h"

enum signals { notification_expanded, notification_collapsed, signals_n };

typedef struct _NotificationWidget {
    GObject parent_instance;
    guint32 id;
    AdwAnimation *expand_animation;
    GtkEventControllerMotion *ctrl;
    GtkBox *container;
    GtkBox *notification_container;
    GtkBox *stack_effect_box1;
    GtkBox *stack_effect_box2;

    // header
    GtkCenterBox *header;
    GtkImage *header_app_icon;
    GtkLabel *header_app_name;
    GtkLabel *header_timer;
    GtkButton *header_expand;
    GtkButton *header_dismiss;

    // notification button
    GtkBox *button_container;
    GtkBox *button_contents;
    GtkButton *button;
    AdwAvatar *avatar;
    GtkImage *icon;
    // button text
    GtkBox *text;
    GtkLabel *summary;
    GtkLabel *body;

    // media player buttons, if notification is a media player.
    GtkBox *media_buttons;
    GtkButton *play_pause;
    GtkButton *previous;
    GtkButton *next;

    // properties
    gboolean expanded;
    GDateTime *created_on;
    guint32 timer_id;
    // mpris media player name, if null, notification is not a media player.
    gchar *media_player_name;
} NotificationWidget;

static guint notification_widget_signals[signals_n] = {0};
G_DEFINE_TYPE(NotificationWidget, notification_widget, G_TYPE_OBJECT);

void on_pointer_enter(GtkEventControllerMotion *ctrl, double x, double y,
                      NotificationWidget *self) {
    g_debug("notification_widget.c:on_pointer_enter() called");
    AdwAnimationState state = adw_animation_get_state(self->expand_animation);
    gboolean reverse = adw_timed_animation_get_reverse(
        ADW_TIMED_ANIMATION(self->expand_animation));

    switch (state) {
        case ADW_ANIMATION_IDLE:
            g_debug("notification_widget.c:on_pointer_enter() called: idle");
            adw_animation_play(self->expand_animation);
            return;
        case ADW_ANIMATION_PAUSED:
            g_debug("notification_widget.c:on_pointer_enter() called: paused");
            adw_timed_animation_set_reverse(
                ADW_TIMED_ANIMATION(self->expand_animation), false);
            adw_animation_play(self->expand_animation);
            return;
        case ADW_ANIMATION_FINISHED:
            g_debug(
                "notification_widget.c:on_pointer_enter() called: finished");
            if (reverse) {
                g_debug(
                    "notification_widget.c:on_pointer_enter() called: finished "
                    "reverse");
                adw_timed_animation_set_reverse(
                    ADW_TIMED_ANIMATION(self->expand_animation), false);
                adw_animation_play(self->expand_animation);
                return;
            }
        case ADW_ANIMATION_PLAYING:
            g_debug(
                "notification_widget.c:on_pointer_enter() called: finished");
            if (reverse) {
                // pause animation
                adw_animation_pause(self->expand_animation);
                // reverse it
                adw_timed_animation_set_reverse(
                    ADW_TIMED_ANIMATION(self->expand_animation), false);
                // play it
                adw_animation_play(self->expand_animation);
                return;
            }
        default:
            return;
    }
}

void on_pointer_leave(GtkEventControllerMotion *ctrl, double x, double y,
                      NotificationWidget *self) {
    AdwAnimationState state = adw_animation_get_state(self->expand_animation);
    gboolean reverse = adw_timed_animation_get_reverse(
        ADW_TIMED_ANIMATION(self->expand_animation));

    switch (state) {
        case ADW_ANIMATION_IDLE:
            // play animation
            adw_animation_play(self->expand_animation);
            return;
        case ADW_ANIMATION_PAUSED:
            adw_timed_animation_set_reverse(
                ADW_TIMED_ANIMATION(self->expand_animation), true);
            adw_animation_play(self->expand_animation);
            return;
        case ADW_ANIMATION_FINISHED:
            if (!reverse) {
                adw_timed_animation_set_reverse(
                    ADW_TIMED_ANIMATION(self->expand_animation), true);
                adw_animation_play(self->expand_animation);
                return;
            }
        case ADW_ANIMATION_PLAYING:
            if (!reverse) {
                // pause animation
                adw_animation_pause(self->expand_animation);
                // reverse it
                adw_timed_animation_set_reverse(
                    ADW_TIMED_ANIMATION(self->expand_animation), true);
                // play it
                adw_animation_play(self->expand_animation);
                return;
            }
        default:
            return;
    }
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

static void on_message_tray_will_hide(MessageTray *tray,
                                      NotificationWidget *self);

// stub out dispose, finalize, class_init and init methods.
static void notification_widget_dispose(GObject *gobject) {
    NotificationWidget *self = NOTIFICATION_WIDGET(gobject);

    // debug with pointer value as hex
    g_debug("notification_widget.c:notification_widget_dispose() called: %p",
            self);

    // remove signal handlers
    g_signal_handlers_disconnect_by_func(self->button, on_notification_clicked,
                                         self);
    g_signal_handlers_disconnect_by_func(self->header_dismiss,
                                         on_dismiss_clicked, self);
    g_signal_handlers_disconnect_by_func(self->ctrl, on_pointer_enter, self);
    g_signal_handlers_disconnect_by_func(self->ctrl, on_pointer_leave, self);

    MessageTray *mt = message_tray_get_global();
    g_signal_handlers_disconnect_by_func(mt, on_message_tray_will_hide, self);

    // kill timer
    g_source_remove(self->timer_id);

    // unref our ref'd datetime.
    g_date_time_unref(self->created_on);

    // Chain-up
    G_OBJECT_CLASS(notification_widget_parent_class)->dispose(gobject);
};

static void notification_widget_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(notification_widget_parent_class)->finalize(gobject);
};

static void notification_widget_class_init(NotificationWidgetClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = notification_widget_dispose;
    object_class->finalize = notification_widget_finalize;

    notification_widget_signals[notification_expanded] =
        g_signal_new("notification-expanded", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    notification_widget_signals[notification_collapsed] =
        g_signal_new("notification-collapsed", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
};

static void expand_animation_cb(double value, GtkLabel *body) {
    gtk_label_set_lines(body, value);
}

static void on_expand_button_clicked(GtkButton *button,
                                     NotificationWidget *self);

static void on_expand_animation_done(AdwAnimation *animation,
                                     NotificationWidget *self) {
    g_debug("notification_widget.c:on_expand_animation_done() called");

    gboolean reverse =
        adw_timed_animation_get_reverse(ADW_TIMED_ANIMATION(animation));

    if (reverse) {
        g_debug(
            "notification_widget.c:on_expand_animation_done() called: "
            "reverse");
        g_signal_emit(self, notification_widget_signals[notification_collapsed],
                      0);
    } else {
        g_debug(
            "notification_widget.c:on_expand_animation_done() called: "
            "not reverse");
        g_signal_emit(self, notification_widget_signals[notification_expanded],
                      0);
    }
}

static void on_message_tray_will_hide(MessageTray *tray,
                                      NotificationWidget *self) {
    g_debug("notification_widget.c:on_message_tray_will_hide() called");
    if (self->expanded) {
        gtk_button_set_icon_name(self->header_expand, "go-down-symbolic");
        adw_timed_animation_set_reverse(
            ADW_TIMED_ANIMATION(self->expand_animation), true);
        adw_animation_play(self->expand_animation);
        self->expanded = !self->expanded;
    }
}

static void notification_widget_init_layout(NotificationWidget *self) {
    // main container for widget
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->container),
                             "notification-widget-container");

    self->notification_container =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->notification_container),
                             "notification-widget");

    // set size request
    gtk_widget_set_size_request(GTK_WIDGET(self->container), 400, 120);

    // motion controller to get mouse in/out events
    self->ctrl = GTK_EVENT_CONTROLLER_MOTION(gtk_event_controller_motion_new());
    gtk_widget_add_controller(GTK_WIDGET(self->container),
                              GTK_EVENT_CONTROLLER(self->ctrl));

    // setup header
    self->header = GTK_CENTER_BOX(gtk_center_box_new());
    gtk_widget_add_css_class(GTK_WIDGET(self->header),
                             "notification-widget-header");
    GtkBox *header_left = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_valign(GTK_WIDGET(header_left), GTK_ALIGN_CENTER);

    GtkBox *header_right = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_valign(GTK_WIDGET(header_right), GTK_ALIGN_CENTER);

    gtk_center_box_set_start_widget(self->header, GTK_WIDGET(header_left));
    gtk_center_box_set_end_widget(self->header, GTK_WIDGET(header_right));

    self->header_app_icon = GTK_IMAGE(gtk_image_new());
    gtk_widget_add_css_class(GTK_WIDGET(self->header_app_icon),
                             "notification-widget-app-icon");
    gtk_image_set_pixel_size(self->header_app_icon, 18);
    gtk_widget_set_halign(GTK_WIDGET(self->header_app_icon), GTK_ALIGN_START);
    gtk_widget_add_css_class(GTK_WIDGET(self->header_app_icon),
                             "notification-widget-icon");

    self->header_app_name = GTK_LABEL(gtk_label_new(""));
    gtk_widget_add_css_class(GTK_WIDGET(self->header_app_name),
                             "notification-widget-app-name");
    gtk_widget_set_valign(GTK_WIDGET(self->header_app_name), GTK_ALIGN_CENTER);

    self->header_timer = GTK_LABEL(gtk_label_new("Just now"));
    gtk_widget_add_css_class(GTK_WIDGET(self->header_timer),
                             "notification-widget-timer");
    gtk_widget_set_valign(GTK_WIDGET(self->header_timer), GTK_ALIGN_END);

    self->header_expand =
        GTK_BUTTON(gtk_button_new_from_icon_name("go-down-symbolic"));
    gtk_widget_add_css_class(GTK_WIDGET(self->header_expand), "circular");
    gtk_widget_add_css_class(GTK_WIDGET(self->header_expand),
                             "notification-widget-expand-button");
    gtk_widget_set_visible(GTK_WIDGET(self->header_expand), false);
    g_signal_connect(self->header_expand, "clicked",
                     G_CALLBACK(on_expand_button_clicked), self);

    GtkImage *header_dismiss_icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("window-close-symbolic"));
    gtk_image_set_pixel_size(header_dismiss_icon, 18);
    self->header_dismiss = GTK_BUTTON(gtk_button_new());
    gtk_button_set_child(self->header_dismiss, GTK_WIDGET(header_dismiss_icon));
    gtk_widget_add_css_class(GTK_WIDGET(self->header_dismiss), "circular");
    gtk_widget_add_css_class(GTK_WIDGET(self->header_dismiss),
                             "notification-widget-dismiss-button");

    gtk_box_append(header_left, GTK_WIDGET(self->header_app_icon));
    gtk_box_append(header_left, GTK_WIDGET(self->header_app_name));
    gtk_box_append(header_left, GTK_WIDGET(self->header_timer));
    gtk_box_append(header_right, GTK_WIDGET(self->header_expand));
    gtk_box_append(header_right, GTK_WIDGET(self->header_dismiss));

    self->button_container =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

    self->button_contents = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

    self->button = GTK_BUTTON(gtk_button_new());
    gtk_button_set_child(self->button, GTK_WIDGET(self->button_contents));
    gtk_widget_add_css_class(GTK_WIDGET(self->button),
                             "notification-widget-button");

    gtk_box_append(self->button_container, GTK_WIDGET(self->button));

    self->avatar = ADW_AVATAR(adw_avatar_new(48, "", false));
    gtk_widget_add_css_class(GTK_WIDGET(self->avatar),
                             "notification-widget-icon");
    adw_avatar_set_icon_name(self->avatar,
                             "preferences-system-notifications-symbolic");
    gtk_widget_set_valign(GTK_WIDGET(self->avatar), GTK_ALIGN_START);

    self->text = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_valign(GTK_WIDGET(self->text), GTK_ALIGN_CENTER);

    // setup summary and body labels
    self->summary = GTK_LABEL(gtk_label_new(""));
    gtk_widget_add_css_class(GTK_WIDGET(self->summary), "summary");
    gtk_widget_set_halign(GTK_WIDGET(self->summary), GTK_ALIGN_START);
    gtk_label_set_max_width_chars(self->summary, 40);
    gtk_label_set_ellipsize(self->summary, PANGO_ELLIPSIZE_END);
    gtk_label_set_lines(self->summary, 1);
    gtk_label_set_wrap(self->summary, true);
    gtk_widget_set_size_request(GTK_WIDGET(self->summary), 380, -1);
    gtk_label_set_xalign(self->summary, 0.0);

    self->body = GTK_LABEL(gtk_label_new(""));
    gtk_widget_add_css_class(GTK_WIDGET(self->body), "body");
    gtk_widget_set_halign(GTK_WIDGET(self->body), GTK_ALIGN_START);
    gtk_label_set_max_width_chars(self->body, 200);
    gtk_label_set_ellipsize(self->body, PANGO_ELLIPSIZE_END);
    gtk_label_set_lines(self->body, 1);
    gtk_label_set_wrap(self->body, true);
    gtk_widget_set_size_request(GTK_WIDGET(self->body), 380, -1);
    gtk_label_set_xalign(self->body, 0.0);

    gtk_box_append(self->text, GTK_WIDGET(self->summary));
    gtk_box_append(self->text, GTK_WIDGET(self->body));

    gtk_box_append(self->button_contents, GTK_WIDGET(self->avatar));
    gtk_box_append(self->button_contents, GTK_WIDGET(self->text));

    // set body expand animation
    AdwAnimationTarget *target = adw_callback_animation_target_new(
        (AdwAnimationTargetFunc)expand_animation_cb, self->body, NULL);
    self->expand_animation =
        adw_timed_animation_new(GTK_WIDGET(self->body), 1, 10, 200, target);
    adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(self->expand_animation),
                                   ADW_LINEAR);

    g_signal_connect(self->expand_animation, "done",
                     G_CALLBACK(on_expand_animation_done), self);

    gtk_box_append(self->notification_container, GTK_WIDGET(self->header));
    gtk_box_append(self->notification_container,
                   GTK_WIDGET(self->button_container));

    gtk_box_append(self->container, GTK_WIDGET(self->notification_container));

    // we can make a 'stacked notifications' effect by being clever with some
    // extra boxes in the notification widget.
    self->stack_effect_box1 = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->stack_effect_box1),
                             "notification-group-stack-effect-box1");
    GtkLabel *label1 = GTK_LABEL(gtk_label_new(""));
    // set width request to 1, allows us to set margins on surrounding box
    gtk_widget_set_size_request(GTK_WIDGET(label1), 1, -1);
    gtk_box_append(self->stack_effect_box1, GTK_WIDGET(label1));
    gtk_widget_set_margin_start(GTK_WIDGET(self->stack_effect_box1), 5);
    gtk_widget_set_margin_end(GTK_WIDGET(self->stack_effect_box1), 5);

    self->stack_effect_box2 = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->stack_effect_box2),
                             "notification-group-stack-effect-box2");
    GtkLabel *label2 = GTK_LABEL(gtk_label_new(""));
    gtk_widget_set_size_request(GTK_WIDGET(label2), 1, -1);
    gtk_box_append(self->stack_effect_box2, GTK_WIDGET(label2));
    gtk_widget_set_margin_start(GTK_WIDGET(self->stack_effect_box2), 10);
    gtk_widget_set_margin_end(GTK_WIDGET(self->stack_effect_box2), 10);

    gtk_box_append(self->container, GTK_WIDGET(self->stack_effect_box1));
    gtk_box_append(self->container, GTK_WIDGET(self->stack_effect_box2));

    // start stack effect boxes as hidden
    gtk_widget_set_visible(GTK_WIDGET(self->stack_effect_box1), false);
    gtk_widget_set_visible(GTK_WIDGET(self->stack_effect_box2), false);
}

static void notification_widget_init(NotificationWidget *self) {
    self->id = 0;
    self->expanded = false;
    self->expand_animation = NULL;
}

static void avatar_from_img_data(NotificationWidget *self,
                                 NotificationImageData *img_data) {
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data(
        (const guchar *)img_data->data,
        img_data->has_alpha ? GDK_COLORSPACE_RGB : GDK_COLORSPACE_RGB,
        img_data->has_alpha, img_data->bits_per_sample, img_data->width,
        img_data->height, img_data->rowstride, NULL, NULL);
    if (pixbuf) {
        GdkPixbuf *scaled_pixbuf =
            gdk_pixbuf_scale_simple(pixbuf, 48, 48, GDK_INTERP_BILINEAR);

        GdkTexture *texture = gdk_texture_new_for_pixbuf(scaled_pixbuf);
        adw_avatar_set_custom_image(self->avatar, GDK_PAINTABLE(texture));
        g_object_unref(texture);
    }
}

static void icon_from_app_id(GtkImage *icon, gchar *app_id) {
    GAppInfo *app_info = NULL;
    GList *apps = g_app_info_get_all();
    GList *l;
    g_debug("app_switcher_app_widget: search_apps_by_app_id: %s", app_id);
    for (l = apps; l != NULL; l = l->next) {
        GAppInfo *info = l->data;
        const gchar *id = g_app_info_get_id(info);
        const gchar *lower_id = g_utf8_strdown(id, -1);
        const gchar *lower_app_id = g_utf8_strdown(app_id, -1);
        if (g_strrstr(lower_id, lower_app_id)) {
            app_info = info;
            break;
        }
    }
    g_list_free(apps);

    GIcon *g_icon = g_app_info_get_icon(G_APP_INFO(app_info));
    if (g_icon && G_IS_THEMED_ICON(g_icon)) {
        GdkDisplay *display = gdk_display_get_default();
        GtkIconTheme *theme = gtk_icon_theme_get_for_display(display);
        GtkIconPaintable *paintable = gtk_icon_theme_lookup_by_gicon(
            theme, g_icon, 48, 1, GTK_TEXT_DIR_RTL, 0);
        gtk_image_set_from_paintable(icon, GDK_PAINTABLE(paintable));
    }
}
static void avatar_from_app_id(NotificationWidget *self, gchar *app_id) {
    GAppInfo *app_info = NULL;
    GList *apps = g_app_info_get_all();
    GList *l;
    g_debug("app_switcher_app_widget: search_apps_by_app_id: %s", app_id);
    for (l = apps; l != NULL; l = l->next) {
        GAppInfo *info = l->data;
        const gchar *id = g_app_info_get_id(info);
        const gchar *lower_id = g_utf8_strdown(id, -1);
        const gchar *lower_app_id = g_utf8_strdown(app_id, -1);
        if (g_strrstr(lower_id, lower_app_id)) {
            app_info = info;
            break;
        }
    }
    g_list_free(apps);

    GIcon *g_icon = g_app_info_get_icon(G_APP_INFO(app_info));
    if (g_icon && G_IS_THEMED_ICON(g_icon)) {
        GdkDisplay *display = gdk_display_get_default();
        GtkIconTheme *theme = gtk_icon_theme_get_for_display(display);
        GtkIconPaintable *paintable = gtk_icon_theme_lookup_by_gicon(
            theme, g_icon, 48, 1, GTK_TEXT_DIR_RTL, 0);
        adw_avatar_set_custom_image(self->avatar, GDK_PAINTABLE(paintable));
    }
}

static void set_notification_icon(NotificationWidget *self, Notification *n) {
    if (n->img_data.data) {
        avatar_from_img_data(self, &n->img_data);
    } else if (n->app_name && (strlen(n->app_name) > 0)) {
        avatar_from_app_id(self, n->app_name);
    } else if (n->desktop_entry && (strlen(n->desktop_entry) > 0)) {
        avatar_from_app_id(self, n->desktop_entry);
    }

    // if this is an internal notification, app_icon will have a string for
    // the system theme icon to use, prefer this.
    if (n->is_internal && (strlen(n->app_icon) > 0)) {
        adw_avatar_set_icon_name(self->avatar, n->app_icon);
    }
}

static void set_text_with_markup(GtkLabel *label, const gchar *text) {
    PangoAttrList *attrs = NULL;
    gchar *buf = NULL;
    GError *error = NULL;

    // Escape the text to ensure it's safe for parsing as markup
    gchar *escaped_text = g_markup_escape_text(text, -1);

    // Try to parse the escaped text as Pango markup
    if (!pango_parse_markup(escaped_text, -1, 0, &attrs, &buf, NULL, &error)) {
        fprintf(stderr, "Could not parse Pango markup %s: %s\n", text,
                error->message);
        g_error_free(error);
        gtk_label_set_text(label, escaped_text);  // Fallback to plain text
    } else {
        gtk_label_set_markup(label, buf);
        if (attrs) {
            gtk_label_set_attributes(label, attrs);
            pango_attr_list_unref(attrs);
        }
    }

    g_free(buf);
    g_free(escaped_text);
}

static void set_notification_text(NotificationWidget *self, Notification *n) {
    // make summary
    char *summary_text = g_strstrip(n->summary);
    summary_text = g_strdelimit(summary_text, "\n", ' ');
    gtk_label_set_text(self->summary, summary_text);

    // make body
    char *body_text = g_strstrip(n->body);
    body_text = g_strdelimit(body_text, "\n", ' ');
    set_text_with_markup(self->body, body_text);
}

static void set_notification_app_icon(NotificationWidget *self,
                                      Notification *n) {
    GtkImage *icon = GTK_IMAGE(gtk_image_new_from_icon_name(
        "preferences-system-notifications-symbolic"));

    gtk_image_set_from_icon_name(icon,
                                 "preferences-system-notifications-symbolic");

    if (n->app_name && (strlen(n->app_name) > 0)) {
        icon_from_app_id(self->header_app_icon, n->app_name);
    } else if (n->desktop_entry && (strlen(n->desktop_entry) > 0)) {
        icon_from_app_id(self->header_app_icon, n->desktop_entry);
    } else if (n->is_internal && (strlen(n->app_icon) > 0)) {
        // if this is an internal notification, app_icon will have a string for
        // the system theme icon to use, prefer this.
        gtk_image_set_from_icon_name(self->header_app_icon, n->app_icon);
    }
}

static void on_expand_button_clicked(GtkButton *button,
                                     NotificationWidget *self) {
    g_debug("notification_widget.c:on_expand_button_clicked() called");

    if (self->expanded) {
        gtk_button_set_icon_name(self->header_expand, "go-down-symbolic");
        adw_timed_animation_set_reverse(
            ADW_TIMED_ANIMATION(self->expand_animation), true);
        adw_animation_play(self->expand_animation);
    } else {
        gtk_button_set_icon_name(self->header_expand, "go-up-symbolic");
        adw_timed_animation_set_reverse(
            ADW_TIMED_ANIMATION(self->expand_animation), false);
        adw_animation_play(self->expand_animation);
    }

    self->expanded = !self->expanded;
}

static gboolean update_timer(NotificationWidget *self) {
    GDateTime *now = g_date_time_new_now_local();

    // determine how many minutes and hours and days have passed
    GTimeSpan span = g_date_time_difference(now, self->created_on);
    gint days = span / G_TIME_SPAN_DAY;
    gint hours = (span % G_TIME_SPAN_DAY) / G_TIME_SPAN_HOUR;
    gint minutes = (span % G_TIME_SPAN_HOUR) / G_TIME_SPAN_MINUTE;

    if (days == 1) {
        gtk_label_set_text(self->header_timer,
                           g_strdup_printf("%d day ago", days));
    } else if (days > 1) {
        gtk_label_set_text(self->header_timer,
                           g_strdup_printf("%d days ago", days));
    } else if (hours == 1) {
        gtk_label_set_text(self->header_timer,
                           g_strdup_printf("%d hour ago", hours));
    } else if (hours > 1) {
        gtk_label_set_text(self->header_timer,
                           g_strdup_printf("%d hours ago", hours));
    } else if (minutes == 1) {
        gtk_label_set_text(self->header_timer,
                           g_strdup_printf("%d minute ago", minutes));
    } else if (minutes > 1) {
        gtk_label_set_text(self->header_timer,
                           g_strdup_printf("%d minutes ago", minutes));
    } else {
        gtk_label_set_text(self->header_timer, "Just now");
    }

    g_date_time_unref(now);

    return true;
}

NotificationWidget *notification_widget_from_notification(
    Notification *n, gboolean expand_on_enter) {
    NotificationWidget *self = g_object_new(NOTIFICATION_WIDGET_TYPE, NULL);

    self->id = n->id;

    notification_widget_init_layout(self);

    g_object_set_data(G_OBJECT(self->container), "notification-id",
                      GUINT_TO_POINTER(n->id));

    if (n->urgency == 2) {
        gtk_widget_add_css_class(GTK_WIDGET(self->button),
                                 "notification-widget-button-critical");
    }

    set_notification_app_icon(self, n);

    gtk_label_set_text(self->header_app_name, n->app_name ? n->app_name : "");

    set_notification_icon(self, n);
    set_notification_text(self, n);

    // notification may provide a created_on field, we can seed our timer
    // value with this.
    self->created_on = g_date_time_ref(n->created_on);
    update_timer(self);

    // wire up notification click
    g_signal_connect(self->button, "clicked",
                     G_CALLBACK(on_notification_clicked), self);

    // wire up dissmiss click
    g_signal_connect(self->header_dismiss, "clicked",
                     G_CALLBACK(on_dismiss_clicked), self);

    // wire up motion controller
    if (expand_on_enter) {
        g_signal_connect(self->ctrl, "enter", G_CALLBACK(on_pointer_enter),
                         self);
        g_signal_connect(self->ctrl, "leave", G_CALLBACK(on_pointer_leave),
                         self);
    } else {
        // set expander button visible as visible if we aren't expanding on
        // cursor enter
        gtk_widget_set_visible(GTK_WIDGET(self->header_expand), true);
    }

    if (!expand_on_enter) {
        MessageTray *mt = message_tray_get_global();
        g_signal_connect(mt, "message-tray-will-hide",
                         G_CALLBACK(on_message_tray_will_hide), self);
    }

    // give the main container a pointer to ourselves.
    g_object_set_data(G_OBJECT(self->container), "self", self);

    // setup timer for ever minute to update header_timer
    self->timer_id = g_timeout_add_seconds(60, (GSourceFunc)update_timer, self);

    return self;
}

GtkWidget *notification_widget_get_widget(NotificationWidget *self) {
    return GTK_WIDGET(self->container);
}

guint32 notification_widget_get_id(NotificationWidget *self) {
    return self->id;
}

GtkLabel *notification_widget_get_summary(NotificationWidget *self) {
    return self->summary;
}

GtkLabel *notification_widget_get_body(NotificationWidget *self) {
    return self->body;
}

void notification_widget_set_stack_effect(NotificationWidget *self,
                                          gboolean show) {
    gtk_widget_set_visible(GTK_WIDGET(self->header_expand), !show);
    gtk_widget_set_visible(GTK_WIDGET(self->stack_effect_box1), show);
    gtk_widget_set_visible(GTK_WIDGET(self->stack_effect_box2), show);
}

void notification_widget_dismiss_notification(NotificationWidget *self) {
    NotificationsService *service = notifications_service_get_global();
    notifications_service_closed_notification(
        service, self->id, NOTIFICATIONS_CLOSED_REASON_DISMISSED);
}

// Media Player Notification Widget

static void media_player_widget_on_playpause_clicked(GtkButton *button,
                                                     gpointer user_data) {
    NotificationWidget *self = (NotificationWidget *)user_data;
    MediaPlayerService *srv = media_player_service_get_global();
    media_player_service_player_playpause(srv, self->media_player_name);
}

static void media_player_widget_on_previous_clicked(GtkButton *button,
                                                    gpointer user_data) {
    NotificationWidget *self = (NotificationWidget *)user_data;
    MediaPlayerService *srv = media_player_service_get_global();
    media_player_service_player_previous(srv, self->media_player_name);
}

static void media_player_widget_on_next_clicked(GtkButton *button,
                                                gpointer user_data) {
    NotificationWidget *self = (NotificationWidget *)user_data;
    MediaPlayerService *srv = media_player_service_get_global();
    media_player_service_player_next(srv, self->media_player_name);
}

static void media_player_widget_on_raise(GtkButton *button,
                                         gpointer user_data) {
    NotificationWidget *self = (NotificationWidget *)user_data;
    MediaPlayerService *srv = media_player_service_get_global();
    media_player_service_player_raise(srv, self->media_player_name);
}

static void on_media_player_img_loaded(GObject *obj, GAsyncResult *res,
                                       gpointer user_data) {
    g_debug("media_players.c:on_image_loaded() called.");

    GFileInputStream *file_input_stream;
    GError *error = NULL;
    GdkPixbuf *pixbuf;

    NotificationWidget *widget = (NotificationWidget *)user_data;

    file_input_stream = g_file_read_finish(G_FILE(obj), res, &error);
    if (error) {
        g_warning("Error loading image: %s", error->message);
        return;
    }

    pixbuf = gdk_pixbuf_new_from_stream(G_INPUT_STREAM(file_input_stream), NULL,
                                        &error);
    if (error) {
        g_warning("Failed to create pixbuf from stream: %s", error->message);
        g_object_unref(file_input_stream);
        return;
    }

    GdkTexture *texture = gdk_texture_new_for_pixbuf(pixbuf);

    adw_avatar_set_custom_image(widget->avatar, GDK_PAINTABLE(texture));

    g_object_unref(file_input_stream);
    g_object_unref(pixbuf);
    g_object_unref(texture);
}

NotificationWidget *notification_widget_set_media_player(
    NotificationWidget *self, MediaPlayer *player) {
    if (player->art_url) {
        GFile *file;
        file = g_file_new_for_uri(player->art_url);
        g_file_read_async(file, G_PRIORITY_DEFAULT, NULL,
                          on_media_player_img_loaded, self);
    }

    // update play/pause icon depending on playback state
    if (g_strcmp0(player->playback_status, "Playing") == 0) {
        gtk_button_set_icon_name(self->play_pause,
                                 "media-playback-pause-symbolic");
    } else {
        gtk_button_set_icon_name(self->play_pause,
                                 "media-playback-start-symbolic");
    }

    gtk_label_set_text(self->header_app_name, player->identity);
    gtk_label_set_text(self->summary, player->artist);
    gtk_label_set_text(self->body, player->title);
}

NotificationWidget *notification_widget_from_media_player(MediaPlayer *player) {
    NotificationWidget *self = g_object_new(NOTIFICATION_WIDGET_TYPE, NULL);

    self->media_player_name = g_strdup(player->name);

    // use almost all of the normal notification's layout, we'll tweak it below.
    notification_widget_init_layout(self);

    // hide dismiss button
    gtk_widget_set_visible(GTK_WIDGET(self->header_dismiss), false);
    // hide header's timer
    gtk_widget_set_visible(GTK_WIDGET(self->header_timer), false);

    // add media-player class to container to css can apply select styling
    gtk_widget_add_css_class(GTK_WIDGET(self->container), "media-player");

    // if we have an identity of the player we can display it in the
    // notification header.
    if (player->identity)
        icon_from_app_id(self->header_app_icon, player->identity);

    // reset some label layout details since media player buttons will be
    // present next to the labels.
    gtk_widget_set_size_request(GTK_WIDGET(self->summary), -1, -1);
    gtk_widget_set_size_request(GTK_WIDGET(self->body), -1, -1);

    gtk_label_set_width_chars(self->summary, 34);
    gtk_label_set_max_width_chars(self->summary, 34);

    gtk_label_set_width_chars(self->body, 34);
    gtk_label_set_max_width_chars(self->body, 34);

    // create the player's media buttons.
    self->media_buttons = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->media_buttons),
                             "notification-widget-media-buttons-container");
    gtk_widget_set_halign(GTK_WIDGET(self->media_buttons), GTK_ALIGN_END);

    self->play_pause = GTK_BUTTON(
        gtk_button_new_from_icon_name("media-playback-pause-symbolic"));
    gtk_widget_add_css_class(GTK_WIDGET(self->play_pause),
                             "notification-widget-media-button");

    self->previous =
        GTK_BUTTON(gtk_button_new_from_icon_name("go-previous-symbolic"));
    gtk_widget_add_css_class(GTK_WIDGET(self->previous),
                             "notification-widget-media-button");

    self->next = GTK_BUTTON(gtk_button_new_from_icon_name("go-next-symbolic"));
    gtk_widget_add_css_class(GTK_WIDGET(self->next),
                             "notification-widget-media-button");

    // wire them up
    g_signal_connect(self->button, "clicked",
                     G_CALLBACK(media_player_widget_on_raise), self);
    g_signal_connect(self->play_pause, "clicked",
                     G_CALLBACK(media_player_widget_on_playpause_clicked),
                     self);
    g_signal_connect(self->previous, "clicked",
                     G_CALLBACK(media_player_widget_on_previous_clicked), self);
    g_signal_connect(self->next, "clicked",
                     G_CALLBACK(media_player_widget_on_next_clicked), self);

    gtk_box_append(self->media_buttons, GTK_WIDGET(self->previous));
    gtk_box_append(self->media_buttons, GTK_WIDGET(self->play_pause));
    gtk_box_append(self->media_buttons, GTK_WIDGET(self->next));

    // append media player buttons next to notification button
    gtk_box_append(self->button_container, GTK_WIDGET(self->media_buttons));

    // give the main container a pointer to ourselves.
    g_object_set_data(G_OBJECT(self->container), "self", self);

    return self;
}

gchar *notification_widget_get_media_player_name(NotificationWidget *self) {
    return self->media_player_name;
}
