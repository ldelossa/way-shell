#include <adwaita.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

#include "../panel.h"
#include "calendar/calendar.h"
#include "message_tray.h"
#include "message_tray_mediator.h"

// global MessageTrayMediator.
static MessageTrayMediator *mediator = NULL;

struct _MessageTray {
    GObject parent_instance;
    AdwWindow *win;
    AdwAnimation *animation;
    GtkBox *container;
    AdwWindow *underlay;
    Panel *panel;
    AdwSwitchRow *dnd_switch;
};
G_DEFINE_TYPE(MessageTray, message_tray, G_TYPE_OBJECT);

void static opacity_animation(double value, MessageTray *tray) {
    gtk_widget_set_opacity(GTK_WIDGET(tray->win), value);
}

// stub out dispose, finalize, class_init and init methods.
static void message_tray_dispose(GObject *gobject) {
    MessageTray *self = MESSAGE_TRAY_TRAY(gobject);
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
};

static void animation_close_done(AdwAnimation *animation, MessageTray *tray) {
    g_debug("message_tray.c:animation_close_done() called.");

    // disconnect done signal
    g_signal_handlers_disconnect_by_func(tray->animation, animation_close_done,
                                         tray);

    // make animation forward
    adw_timed_animation_set_reverse(
        ADW_TIMED_ANIMATION(tray->animation), FALSE);

    // hide tray
    gtk_widget_set_visible(GTK_WIDGET(tray->win), false);
    gtk_widget_set_visible(GTK_WIDGET(tray->underlay), false);

    // emit hidden signal
    message_tray_mediator_emit_hidden(mediator, tray, tray->panel);

    // unref and clear panel
    g_clear_object(&tray->panel);
};

void message_tray_set_hidden(MessageTray *self, Panel *panel) {
    int anim_state = 0;

    g_debug("message_tray.c:message_tray_set_hidden() called.");

    // this is required, since external callers will call this method as expect
    // a no-op of there is no attached self->panel.
    if (!self || !self->win || !self->panel || !panel) {
        return;
    }

    // this ensures there are no in-flight animation done callbacks on the
    // event-loop before starting a new animation, and avoids timing bugs.
    anim_state = adw_animation_get_state(self->animation);
    if (anim_state != ADW_ANIMATION_IDLE &&
        anim_state != ADW_ANIMATION_FINISHED) {
        return;
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
    message_tray_set_hidden(tray, tray->panel);
};

static void message_tray_init_underlay(MessageTray *self) {
    self->underlay = ADW_WINDOW(adw_window_new());
    gtk_widget_add_css_class(GTK_WIDGET(self->underlay), "underlay");
    gtk_layer_init_for_window(GTK_WINDOW(self->underlay));
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

static void message_tray_init_layout(MessageTray *self) {
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

    AdwStatusPage *status = ADW_STATUS_PAGE(adw_status_page_new());
    adw_status_page_set_icon_name(status, "notifications-disabled-symbolic");
    adw_status_page_set_title(status, "No Notifications");
    gtk_widget_set_vexpand(GTK_WIDGET(status), true);

    self->dnd_switch = ADW_SWITCH_ROW(adw_switch_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->dnd_switch),
                                  "Do Not Disturb");

    Calendar *calendar = g_object_new(CALENDAR_TYPE, NULL);
    gtk_box_append(right_box, GTK_WIDGET(calendar_get_widget(calendar)));

    gtk_box_append(left_box, GTK_WIDGET(status));
    gtk_box_append(left_box, GTK_WIDGET(self->dnd_switch));

    // set 800x600 size request and adjust left box
    gtk_widget_set_size_request(GTK_WIDGET(self->container), 800, 600);
    gtk_widget_set_size_request(GTK_WIDGET(left_box), 800 * 0.60, 600);

    // configure layer shell properties for window
    gtk_layer_init_for_window(GTK_WINDOW(self->win));
    gtk_layer_set_layer((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_LAYER_OVERLAY);
    adw_window_set_content(self->win, GTK_WIDGET(self->container));
    gtk_widget_set_name(GTK_WIDGET(self->win), "message-tray");
    gtk_layer_set_anchor(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_TOP, true);
    gtk_layer_set_margin(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_TOP, 10);

    // animation controller
    AdwAnimationTarget *target = adw_callback_animation_target_new(
        (AdwAnimationTargetFunc)opacity_animation, self, NULL);
    self->animation =
        adw_timed_animation_new(GTK_WIDGET(self->win), 0, 1, 250, target);
}

static void message_tray_init(MessageTray *self) {
    // make our modal window
    self->win = ADW_WINDOW(adw_window_new());

    // setup the click away window
    message_tray_init_underlay(self);

    // setup the layout
    message_tray_init_layout(self);
};

// global getter for mediator
MessageTrayMediator *message_tray_get_global_mediator() { return mediator; };

void message_tray_activate(AdwApplication *app, gpointer user_data) {
    MessageTray *tray = g_object_new(MESSAGE_TRAY_TYPE, NULL);
    mediator = g_object_new(MESSAGE_TRAY_MEDIATOR_TYPE, NULL);
    message_tray_mediator_set_tray(mediator, tray);
};

static void animation_open_done(AdwAnimation *animation, MessageTray *tray) {
    g_debug("message_tray.c:animation_open_done() called.");

    // disconnect done signal
    g_signal_handlers_disconnect_by_func(tray->animation, animation_open_done,
                                         tray);
    // emit visible signal
    message_tray_mediator_emit_visible(mediator, tray, tray->panel);
};

void message_tray_set_visible(MessageTray *self, Panel *panel) {
    GdkMonitor *monitor = NULL;
    int anim_state = 0;

    g_debug("message_tray.c:message_tray_set_visible() called.");

    if (!self || !panel) return;

    // this ensures there are no in-flight animation done callbacks on the
    // event-loop before starting a new animation, and avoids timing bugs.
    anim_state = adw_animation_get_state(self->animation);
    g_debug("message_tray.c:message_tray_set_visible() anim_state: %d",
            anim_state);
    if (anim_state != ADW_ANIMATION_IDLE &&
        anim_state != ADW_ANIMATION_FINISHED) {
        return;
    }

    // its possible that win is not a valid object, this could be due to the
    // monitor being removed while the top level window is opened.
    if (!G_IS_OBJECT(self->win)) {
        g_debug(
            "message_tray.c:message_tray_set_visible() tray->win is not a "
            "valid GObject must re-initialize.");
        message_tray_init(self);
    }

    monitor = panel_get_monitor(panel);
    if (!monitor) return;

    // move underlay and win to new monitor
    gtk_layer_set_monitor(GTK_WINDOW(self->underlay), monitor);
    gtk_layer_set_monitor(GTK_WINDOW(self->win), monitor);

    // emit will show signal before we open any windows.
    message_tray_mediator_emit_will_show(mediator, self, panel);

    // present underlay
    gtk_window_present(GTK_WINDOW(self->underlay));

    // present the window
    gtk_window_present(GTK_WINDOW(self->win));

    // ref and store the pointer to the panel we attached to.
    g_object_ref(panel);
    self->panel = panel;

    // connect animation done signal and play animation.
    g_signal_connect(self->animation, "done",
                     G_CALLBACK(animation_open_done), self);
    adw_animation_play(self->animation);
}

void message_tray_toggle(MessageTray *self, Panel *panel) {
    g_debug("message_tray.c:message_tray_toggle called.");

    if (!self || !panel) return;

    if (!self->panel) {
        return message_tray_set_visible(self, panel);
    }
    // tray is open on same panel requesting, hide it.
    if (self->panel == panel) {
        return message_tray_set_hidden(self, panel);
    }
    // tray is displayed on another monitor, hide it first.
    if (self->panel != panel) {
        message_tray_set_hidden(self, self->panel);
    }
    return message_tray_set_visible(self, panel);
}

Panel *message_tray_get_panel(MessageTray *self) {
    return self->panel;
}
