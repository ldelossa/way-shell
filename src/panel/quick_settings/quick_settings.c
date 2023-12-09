#include "quick_settings.h"

#include <adwaita.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>
#include <upower.h>

#include "../../services/upower_service.h"
#include "../panel.h"
#include "gtk/gtkrevealer.h"
#include "quick_settings_audio_scales.h"
#include "quick_settings_battery_button.h"
#include "quick_settings_mediator.h"
#include "quick_settings_power_menu.h"

// global QuickSettingsMediator.
static QuickSettingsMediator *mediator = NULL;

// global QuickSettingsMediator subsystem.
static QuickSettings *qs = NULL;

struct _QuickSettings {
    GObject parent_instance;
    AdwWindow *win;
    AdwAnimation *animation;
    GtkBox *container;
    struct {
        GtkCenterBox *container;
        QuickSettingsBatteryButton bat_button;
        GtkButton *power_button;
    } header;
    QuickSettingsPowerMenu power_menu;
    AudioScales audio_scales;
    AdwWindow *underlay;
    Panel *panel;
};
G_DEFINE_TYPE(QuickSettings, quick_settings, G_TYPE_OBJECT);

void static opacity_animation(double value, QuickSettings *qs) {
    gtk_widget_set_opacity(GTK_WIDGET(qs->win), value);
}

// stub out dispose, finalize, class_init and init methods.
static void quick_settings_dispose(GObject *gobject) {
    QuickSettings *qs = QS_QUICK_SETTINGS(gobject);

    // free embedded battery button widgets
    quick_settings_battery_button_free(&qs->header.bat_button);

    // if we are holding a panel un-ref it
    if (qs->panel) {
        g_object_unref(qs->panel);
    }

    // Chain-up
    G_OBJECT_CLASS(quick_settings_parent_class)->dispose(gobject);
};

static void quick_settings_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(quick_settings_parent_class)->finalize(gobject);
};

static void quick_settings_class_init(QuickSettingsClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = quick_settings_dispose;
    object_class->finalize = quick_settings_finalize;
};

static void animation_close_done(AdwAnimation *animation, QuickSettings *qs) {
    g_debug("quick_settings.c:animation_close_done() called.");

    // disconnect done signal
    g_signal_handlers_disconnect_by_func(qs->animation, animation_close_done,
                                         qs);

    // make animation forward
    adw_timed_animation_set_reverse(ADW_TIMED_ANIMATION(qs->animation), FALSE);

    // hide qs
    gtk_widget_set_visible(GTK_WIDGET(qs->win), false);
    gtk_widget_set_visible(GTK_WIDGET(qs->underlay), false);

    // emit hidden signal
    quick_settings_mediator_emit_hidden(mediator, qs, qs->panel);

    // unref and clear panel
    g_clear_object(&qs->panel);
};

void quick_settings_set_hidden(QuickSettings *self, Panel *panel) {
    int anim_state = 0;

    g_debug("quick_settings.c:quick_settings_set_hidden() called.");

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

static void on_underlay_click(GtkButton *button, QuickSettings *qs) {
    g_debug("quick_settings.c:on_click() called.");
    quick_settings_set_hidden(qs, qs->panel);
};

static void quick_settings_init_underlay(QuickSettings *self) {
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

static void on_power_button_click(GtkButton *button, QuickSettings *qs) {
    g_debug("quick_settings.c:on_power_button_click() called.");

    gboolean revealed =
        gtk_revealer_get_reveal_child(GTK_REVEALER(qs->power_menu.revealer));
    gtk_revealer_set_reveal_child(qs->power_menu.revealer, !revealed);

    if (revealed) {
        gtk_window_set_default_size(GTK_WINDOW(qs->win), 400, 200);
    }
};

static void quick_settings_init_layout_header_power_button(
    QuickSettings *self) {
    self->header.power_button = GTK_BUTTON(gtk_button_new());

    gtk_widget_add_css_class(GTK_WIDGET(self->header.power_button), "circular");

    GtkWidget *icon = gtk_image_new_from_icon_name("system-shutdown-symbolic");

    gtk_button_set_child(self->header.power_button, icon);

    g_signal_connect(self->header.power_button, "clicked",
                     G_CALLBACK(on_power_button_click), self);
};

static void quick_settings_init_layout_header(QuickSettings *self) {
    // init embedded widgets
    quick_settings_battery_button_init(&self->header.bat_button);
    quick_settings_init_layout_header_power_button(self);

    self->header.container = GTK_CENTER_BOX(gtk_center_box_new());
    gtk_widget_set_name(GTK_WIDGET(self->header.container),
                        "quick-settings-header");

    // battery button as start widget of header's center box
    gtk_center_box_set_start_widget(self->header.container,
                                    GTK_WIDGET(self->header.bat_button.button));

    // power button as first end widget of header's center box
    gtk_center_box_set_end_widget(self->header.container,
                                  GTK_WIDGET(self->header.power_button));

    // add header to quick setting's container
    gtk_box_append(self->container, GTK_WIDGET(self->header.container));
}

static void quick_settings_init_layout(QuickSettings *self) {
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_orientable_set_orientation(GTK_ORIENTABLE(self->container),
                                   GTK_ORIENTATION_HORIZONTAL);

    gtk_widget_set_size_request(GTK_WIDGET(self->win), 400, 200);

    // configure layer shell properties of window.
    gtk_layer_init_for_window(GTK_WINDOW(self->win));
    gtk_layer_set_layer((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_LAYER_OVERLAY);
    adw_window_set_content(self->win, GTK_WIDGET(self->container));
    gtk_widget_set_name(GTK_WIDGET(self->win), "quick-settings");

    gtk_layer_set_anchor(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_TOP, true);
    gtk_layer_set_anchor(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_RIGHT,
                         true);
    gtk_layer_set_margin(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_TOP, 10);
    gtk_layer_set_margin(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_RIGHT, 30);

    // create vertical container box
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    adw_window_set_content(self->win, GTK_WIDGET(self->container));

    // add header
    quick_settings_init_layout_header(self);

    // add powermenu
    quick_settings_power_menu_init(&self->power_menu);
    gtk_box_append(GTK_BOX(self->container),
                   GTK_WIDGET(self->power_menu.revealer));

    // add audio scales
    quick_settings_audio_scales_init(&self->audio_scales);
    gtk_box_append(GTK_BOX(self->container),
                   GTK_WIDGET(self->audio_scales.container));

    // animation controller
    AdwAnimationTarget *target = adw_callback_animation_target_new(
        (AdwAnimationTargetFunc)opacity_animation, self, NULL);
    self->animation =
        adw_timed_animation_new(GTK_WIDGET(self->win), 0, 1, 250, target);
};

static void quick_settings_init(QuickSettings *self) {
    self->win = ADW_WINDOW(adw_window_new());

    // setup the click away window
    quick_settings_init_underlay(self);

    // setup the layout
    quick_settings_init_layout(self);
};

static void animation_open_done(AdwAnimation *animation, QuickSettings *qs) {
    g_debug("quick_settings.c:animation_open_done() called.");

    // disconnect done signal
    g_signal_handlers_disconnect_by_func(qs->animation, animation_open_done,
                                         qs);
    // emit visible signal
    quick_settings_mediator_emit_visible(mediator, qs, qs->panel);
};

void quick_settings_set_visible(QuickSettings *self, Panel *panel) {
    GdkMonitor *monitor = NULL;
    int anim_state = 0;

    g_debug("quick_settings.c:quick_settings_set_visible() called.");

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
            "quick_settings.c:quick_settings_set_visible() qs->win is not a "
            "valid GObject must re-initialize.");
        quick_settings_init(self);
    }

    monitor = panel_get_monitor(panel);
    if (!monitor) return;

    // move underlay and win to new monitor
    gtk_layer_set_monitor(GTK_WINDOW(self->underlay), monitor);
    gtk_layer_set_monitor(GTK_WINDOW(self->win), monitor);

    // emit will show signal before we open any windows.
    quick_settings_mediator_emit_will_show(mediator, self, panel);

    // show underlay
    gtk_window_present(GTK_WINDOW(self->underlay));

    // present the window
    gtk_window_present(GTK_WINDOW(self->win));

    // ref and store pointer to panel we attached to.
    g_object_ref(panel);
    self->panel = panel;

    // connect animation done signal and play animation
    g_signal_connect(self->animation, "done", G_CALLBACK(animation_open_done),
                     self);
    adw_animation_play(self->animation);
}

void quick_settings_toggle(QuickSettings *qs, Panel *panel) {
    g_debug("quick_settings.c:quick_settings_toggle() called.");

    if (!qs || !panel) return;

    if (!qs->panel) {
        return quick_settings_set_visible(qs, panel);
    }
    // qs is open on same panel requesting, hide it.
    if (qs->panel == panel) {
        return quick_settings_set_hidden(qs, panel);
    }
    // qs is displayed on another monitor, hide it first.
    if (qs->panel != panel) {
        quick_settings_set_hidden(qs, qs->panel);
    }
    return quick_settings_set_visible(qs, panel);
}

QuickSettingsMediator *quick_settings_get_global_mediator() {
    return mediator;
};

void quick_settings_activate(AdwApplication *app, gpointer user_data) {
    qs = g_object_new(QUICK_SETTINGS_TYPE, NULL);
    mediator = g_object_new(QUICK_SETTINGS_MEDIATOR_TYPE, NULL);
    quick_settings_mediator_set_qs(mediator, qs);
};

Panel *quick_settings_get_panel(QuickSettings *qs) { return qs->panel; };