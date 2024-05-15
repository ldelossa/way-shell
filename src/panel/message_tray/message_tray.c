#include "message_tray.h"

#include <adwaita.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

#include "./notifications/notifications_list.h"
#include "calendar/calendar.h"
#include "gtk/gtk.h"

enum signals {
    // the message tray is about to be visible, but not on screen yet.
    message_tray_will_show,
    // the message tray is now visible.
    message_tray_visible,
    // the message tray is about to be hidden, still on screen.
    message_tray_will_hide,
    // the message tray is now hidden.
    message_tray_hidden,

    signals_n
};

static MessageTray *global;

struct _MessageTray {
    GObject parent_instance;
    AdwWindow *win;
    AdwAnimation *animation;
    GtkBox *container;
    AdwWindow *underlay;
    Calendar *calendar;
    NotificationsList *notifications_list;
};
static guint signals[signals_n] = {0};
G_DEFINE_TYPE(MessageTray, message_tray, G_TYPE_OBJECT);

void static opacity_animation(double value, MessageTray *tray) {
    gtk_widget_set_opacity(GTK_WIDGET(tray->win), value);
}

// stub out dispose, finalize, class_init and init methods.
static void message_tray_dispose(GObject *gobject) {
    MessageTray *self = MESSAGE_TRAY_TRAY(gobject);

    g_object_unref(self->notifications_list);

    // Chain-up
    G_OBJECT_CLASS(message_tray_parent_class)->dispose(gobject);
};

static void message_tray_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(message_tray_parent_class)->finalize(gobject);
};

static void message_tray_class_init(MessageTrayClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = message_tray_dispose;
    object_class->finalize = message_tray_finalize;

    signals[message_tray_will_show] =
        g_signal_new("message-tray-will-show", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    signals[message_tray_visible] =
        g_signal_new("message-tray-visible", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    signals[message_tray_will_hide] =
        g_signal_new("message-tray-will-hide", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    signals[message_tray_hidden] =
        g_signal_new("message-tray-hidden", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
};

static void animation_close_done(AdwAnimation *animation, MessageTray *self) {
    g_debug("message_tray.c:animation_close_done() called.");

    // emit will hide signal
    g_signal_emit(self, signals[message_tray_will_hide], 0);

    // disconnect done signal
    g_signal_handlers_disconnect_by_func(self->animation, animation_close_done,
                                         self);

    // make animation forward
    adw_timed_animation_set_reverse(ADW_TIMED_ANIMATION(self->animation),
                                    FALSE);
    // hide tray
    gtk_widget_set_visible(GTK_WIDGET(self->win), false);
    gtk_widget_set_visible(GTK_WIDGET(self->underlay), false);

    // emit hidden signal
    g_signal_emit(self, signals[message_tray_hidden], 0);

    // after the above signal is emitted, notifications or other child widgets
    // may be removed or collapsed, so shrink the message tray after being
    // hidden, ensuring when its opened itll be back at the default size.
    message_tray_shrink(self);
};

void message_tray_set_hidden(MessageTray *self) {
    int anim_state = 0;

    g_debug("message_tray.c:message_tray_set_hidden() called.");

    // this is required, since external callers will call this method as expect
    // a no-op of there is no attached self->monitor.
    if (!self || !self->win) {
        return;
    }

    // this ensures there are no in-flight animation done callbacks on the
    // event-loop before starting a new animation, and avoids timing bugs.
    anim_state = adw_animation_get_state(self->animation);
    if (anim_state != ADW_ANIMATION_IDLE &&
        anim_state != ADW_ANIMATION_FINISHED) {
        adw_animation_reset(self->animation);
    }

    // reverse animation
    adw_timed_animation_set_reverse(ADW_TIMED_ANIMATION(self->animation), TRUE);

    // connect signal
    g_signal_connect(self->animation, "done", G_CALLBACK(animation_close_done),
                     self);

    // run animation
    adw_animation_play(self->animation);
}

static void on_underlay_click(GtkButton *button, MessageTray *tray) {
    g_debug("message_tray.c:on_click() called.");
    message_tray_set_hidden(tray);
};

static void message_tray_init_underlay(MessageTray *self) {
    self->underlay = ADW_WINDOW(adw_window_new());
    gtk_widget_add_css_class(GTK_WIDGET(self->underlay), "underlay");
    gtk_layer_init_for_window(GTK_WINDOW(self->underlay));
    gtk_layer_set_namespace(GTK_WINDOW(self->underlay),
                            "way-shell-message-tray-underlay");
    gtk_layer_set_layer((GTK_WINDOW(self->underlay)),
                        GTK_LAYER_SHELL_LAYER_TOP);

    gtk_layer_set_anchor(GTK_WINDOW(self->underlay), GTK_LAYER_SHELL_EDGE_TOP,
                         true);
    gtk_layer_set_anchor(GTK_WINDOW(self->underlay),
                         GTK_LAYER_SHELL_EDGE_BOTTOM, true);
    gtk_layer_set_anchor(GTK_WINDOW(self->underlay), GTK_LAYER_SHELL_EDGE_LEFT,
                         true);
    gtk_layer_set_anchor(GTK_WINDOW(self->underlay), GTK_LAYER_SHELL_EDGE_RIGHT,
                         true);

    GtkWidget *button = gtk_button_new();
    gtk_widget_set_hexpand(GTK_WIDGET(button), true);
    gtk_widget_set_vexpand(GTK_WIDGET(button), true);
    adw_window_set_content(self->underlay, GTK_WIDGET(button));

    g_signal_connect(button, "clicked", G_CALLBACK(on_underlay_click), self);
};

static void on_window_destroy(GtkWidget *w, MessageTray *self) {
    g_debug("message_tray.c:on_window_destroy() called.");
    self->win = NULL;
    message_tray_reinitialize(self);
};

static void on_layout_changed(GdkSurface *surface, gint width, gint height,
                              MessageTray *self) {
    g_debug("message_tray.c:on_layout_changed() called. width: %d, height: %d",
            width, height);
}

static void on_message_tray_req_open(MessageTray *self) {
    g_debug("messaget_tray.c:on_message_tray_open() called.");
    message_tray_set_visible(self);
};

static void on_message_tray_req_close(MessageTray *self) {
    g_debug("messaget_tray.c:message_tray_close() called.");
    message_tray_set_hidden(self);
};

static void on_message_tray_req_shrink(MessageTray *self) {
    g_debug("messaget_tray.c:message_tray_shrink() called.");
    message_tray_shrink(self);
};

static void message_tray_init_layout(MessageTray *self) {
    // setup adw window
    self->win = ADW_WINDOW(adw_window_new());
    // configure layer shell properties for window
    gtk_layer_init_for_window(GTK_WINDOW(self->win));
    gtk_layer_set_layer((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_namespace(GTK_WINDOW(self->win), "way-shell-message-tray");
    gtk_widget_set_name(GTK_WIDGET(self->win), "message-tray");
    gtk_layer_set_anchor(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_TOP, true);
    gtk_layer_set_margin(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_TOP, 8);

    // get GdkSurface from self->win
    GtkNative *native = gtk_widget_get_native(GTK_WIDGET(self->win));
    GdkSurface *surface = gtk_native_get_surface(native);
    g_signal_connect(surface, "layout", G_CALLBACK(on_layout_changed), self);

    // wire into window's GtkWidget destroy signal
    g_signal_connect(self->win, "destroy", G_CALLBACK(on_window_destroy), self);

    // setup the click away window
    message_tray_init_underlay(self);

    // make top level container.
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_hexpand_set(GTK_WIDGET(self->container), true);
    gtk_widget_set_vexpand_set(GTK_WIDGET(self->container), true);

    // create three columns
    GtkBox *left_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    GtkSeparator *sep =
        GTK_SEPARATOR(gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    GtkBox *right_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));

    // attach boxes to container
    gtk_box_append(self->container, GTK_WIDGET(left_box));
    gtk_box_append(self->container, GTK_WIDGET(sep));
    gtk_box_append(self->container, GTK_WIDGET(right_box));

    // right box will always use rest of available space.
    gtk_widget_set_hexpand(GTK_WIDGET(right_box), true);
    gtk_widget_set_vexpand(GTK_WIDGET(right_box), true);

    gtk_box_append(
        left_box,
        GTK_WIDGET(notifications_list_get_widget(self->notifications_list)));

    // add calendar widget
    self->calendar = g_object_new(CALENDAR_TYPE, NULL);
    gtk_box_append(right_box, GTK_WIDGET(calendar_get_widget(self->calendar)));

    // set 800x600 size request and adjust left box
    gtk_widget_set_size_request(GTK_WIDGET(self->container), 700, 600);
    gtk_widget_set_size_request(GTK_WIDGET(left_box), 700 * 0.67, 600);

    // animation controller
    AdwAnimationTarget *target = adw_callback_animation_target_new(
        (AdwAnimationTargetFunc)opacity_animation, self, NULL);
    self->animation =
        adw_timed_animation_new(GTK_WIDGET(self->win), 0, 1, 250, target);

    adw_window_set_content(self->win, GTK_WIDGET(self->container));

    // wire up request signals.
    g_signal_connect(self, "message-tray-req-open",
                     G_CALLBACK(on_message_tray_req_open), NULL);

    g_signal_connect(self, "message-tray-req-close",
                     G_CALLBACK(on_message_tray_req_close), NULL);

    g_signal_connect(self, "message-tray-req-shrink",
                     G_CALLBACK(on_message_tray_req_shrink), NULL);
}

void message_tray_reinitialize(MessageTray *self) {
    // reinitialize our depedencies
    notifications_list_reinitialize(self->notifications_list);

    // initialize our layout again
    message_tray_init_layout(self);
}

static void message_tray_init(MessageTray *self) {
    // this is kinda hacky, but we an set the global variable as we init.
    // MessageTray is always a singleton and we don't create multiple instances.
    //
    // this makes constructing dependent components which require the global
    // variable to be set function properly.
    global = self;

    // create dependent widgets
    self->notifications_list = g_object_new(NOTIFICATIONS_LIST_TYPE, NULL);

    g_object_unref(self->calendar);

    // setup the layout
    message_tray_init_layout(self);
};

// global getter for mediator
MessageTray *message_tray_get_global() { return global; };

void message_tray_activate(AdwApplication *app, gpointer user_data) {
    g_object_new(MESSAGE_TRAY_TYPE, NULL);
};

static void animation_open_done(AdwAnimation *animation, MessageTray *tray) {
    g_debug("message_tray.c:animation_open_done() called.");

    // disconnect done signal
    g_signal_handlers_disconnect_by_func(tray->animation, animation_open_done,
                                         tray);
    // emit visible signal
    g_signal_emit(tray, signals[message_tray_visible], 0);
};

void message_tray_set_visible(MessageTray *self) {
    int anim_state = 0;

    g_debug("message_tray.c:message_tray_set_visible() called.");

    if (!self) return;

    // this ensures there are no in-flight animation done callbacks on the
    // event-loop before starting a new animation, and avoids timing bugs.
    anim_state = adw_animation_get_state(self->animation);
    g_debug("message_tray.c:message_tray_set_visible() anim_state: %d",
            anim_state);
    if (anim_state != ADW_ANIMATION_IDLE &&
        anim_state != ADW_ANIMATION_FINISHED) {
        return;
    }

    // emit will show signal before we open any windows.
    g_signal_emit(self, signals[message_tray_will_show], 0);

    // present underlay
    gtk_window_present(GTK_WINDOW(self->underlay));

    // present the window
    gtk_window_present(GTK_WINDOW(self->win));

    // connect animation done signal and play animation.
    g_signal_connect(self->animation, "done", G_CALLBACK(animation_open_done),
                     self);

    adw_animation_play(self->animation);
}

void message_tray_toggle(MessageTray *self) {
    g_debug("message_tray.c:message_tray_toggle() called.");
    if (gtk_widget_get_visible(GTK_WIDGET(self->win))) {
        message_tray_set_hidden(self);
    } else {
        message_tray_set_visible(self);
    }
}

void message_tray_shrink(MessageTray *self) {
    g_debug("message_tray.c:message_tray_shrink() called.");
    gtk_window_set_default_size(GTK_WINDOW(self->win), 700, 600);
}
