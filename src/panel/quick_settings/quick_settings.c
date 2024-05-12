#include "quick_settings.h"

#include <adwaita.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>
#include <upower.h>

#include "quick_settings_grid/quick_settings_grid.h"
#include "quick_settings_header/quick_settings_header.h"
#include "quick_settings_scales/quick_settings_scales.h"

static QuickSettings *global = NULL;

enum signals {
    // signals
    // the quick settings is about to be visible, but not on screen yet.
    quick_settings_will_show,
    // the quick settings is now visible.
    quick_settings_visible,
    // the quick settings is now hidden.
    quick_settings_hidden,

    signals_n
};

struct _QuickSettings {
    GObject parent_instance;
    AdwWindow *win;
    AdwAnimation *animation;
    GtkBox *container;
    QuickSettingsHeader *header;
    QuickSettingsGrid *qs_grid;
    QuickSettingsScales *scales;
    AdwWindow *underlay;
};
static guint signals[signals_n] = {0};
G_DEFINE_TYPE(QuickSettings, quick_settings, G_TYPE_OBJECT);

void static opacity_animation(double value, QuickSettings *qs) {
    gtk_widget_set_opacity(GTK_WIDGET(qs->win), value);
}

// stub out dispose, finalize, class_init and init methods.
static void quick_settings_dispose(GObject *gobject) {
    QuickSettings *self = QS_QUICK_SETTINGS(gobject);

    g_object_unref(self->header);
    g_object_unref(self->qs_grid);
    g_object_unref(self->underlay);
    g_object_unref(self->scales);

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

    signals[quick_settings_will_show] = g_signal_new(
        "quick-settings-will-show", G_TYPE_FROM_CLASS(object_class),
        G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    signals[quick_settings_visible] =
        g_signal_new("quick-settings-visible", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    signals[quick_settings_hidden] =
        g_signal_new("quick-settings-hidden", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
};

static void animation_close_done(AdwAnimation *animation, QuickSettings *self) {
    g_debug("quick_settings.c:animation_close_done() called.");

    // disconnect done signal
    g_signal_handlers_disconnect_by_func(self->animation, animation_close_done,
                                         self);

    // make animation forward
    adw_timed_animation_set_reverse(ADW_TIMED_ANIMATION(self->animation),
                                    FALSE);

    // hide qs
    gtk_widget_set_visible(GTK_WIDGET(self->win), false);
    gtk_widget_set_visible(GTK_WIDGET(self->underlay), false);

    // emit hidden signal
    g_signal_emit(self, signals[quick_settings_hidden], 0);

    quick_settings_set_focused(self, false);
};

void quick_settings_set_hidden(QuickSettings *self) {
    int anim_state = 0;

    g_debug("quick_settings.c:quick_settings_set_hidden() called.");

    // this is required, since external callers will call this method as expect
    // a no-op of there is no attached self->panel.
    if (!self || !self->win) {
        g_debug("quick_settings.c:quick_settings_set_hidden() no-op.");
        return;
    }

    // this ensures there are no in-flight animation done callbacks on the
    // event-loop before starting a new animation, and avoids timing bugs.
    anim_state = adw_animation_get_state(self->animation);
    if (anim_state != ADW_ANIMATION_IDLE &&
        anim_state != ADW_ANIMATION_FINISHED) {
        g_debug("quick_settings.c:quick_settings_set_hidden() anim_state: %d",
                anim_state);
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
    quick_settings_set_hidden(qs);
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

// fwd declare
static void on_window_destroy(GtkWindow *win, QuickSettings *self);

// When the mixer is revealed we want to disable our audio scales, since this
// would show redundant audio sliders.
static void on_mixer_revealed(QuickSettingsHeader *header, gboolean revealed,
                              QuickSettings *self) {
    g_debug("quick_settings.c:on_mixer_revealed() called.");
    if (revealed) {
        quick_settings_scales_disable_audio_scales(self->scales, true);
    } else {
        quick_settings_scales_disable_audio_scales(self->scales, false);
    }
};

static void on_quick_settings_req_open(QuickSettings *self) {
    g_debug("quick_settings.c.c:on_quick_settings_open() called.");
    quick_settings_set_visible(self);
};

static void on_quick_settings_req_close(QuickSettings *self) {
    g_debug("quick_settings.c.c:quick_settings_close() called.");
    quick_settings_set_hidden(self);
};

static void on_quick_settings_req_shrink(QuickSettings *self) {
    g_debug("quick_settings.c.c:quick_settings_shrink() called.");
    quick_settings_shrink(self);
};

static void quick_settings_init_layout(QuickSettings *self) {
    // init underlay
    quick_settings_init_underlay(self);

    // setup ADW window
    self->win = ADW_WINDOW(adw_window_new());
    g_signal_connect(self->win, "destroy", G_CALLBACK(on_window_destroy), self);
    gtk_widget_set_size_request(GTK_WIDGET(self->win), 400, 280);

    // configure layer shell properties of window.
    gtk_layer_init_for_window(GTK_WINDOW(self->win));
    gtk_layer_set_layer((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_widget_set_name(GTK_WIDGET(self->win), "quick-settings");

    gtk_layer_set_anchor(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_TOP, true);
    gtk_layer_set_anchor(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_RIGHT,
                         true);
    gtk_layer_set_margin(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_TOP, 8);
    gtk_layer_set_margin(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_RIGHT, 20);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(self->win),
                                GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);

    // create vertical container box
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));

    // attach header
    gtk_box_append(self->container,
                   quick_settings_header_get_widget(self->header));

    self->scales = g_object_new(QUICK_SETTINGS_SCALES_TYPE, NULL);
    gtk_box_append(self->container,
                   quick_settings_scales_get_widget(self->scales));

    // attach quick settings grid
    gtk_box_append(self->container,
                   quick_settings_grid_get_widget(self->qs_grid));

    // animation controller
    AdwAnimationTarget *target = adw_callback_animation_target_new(
        (AdwAnimationTargetFunc)opacity_animation, self, NULL);
    self->animation =
        adw_timed_animation_new(GTK_WIDGET(self->win), 0, 1, 250, target);

    adw_window_set_content(self->win, GTK_WIDGET(self->container));

    // connect to header's mixer-revealed signal
    g_signal_connect(self->header, "mixer-revealed",
                     G_CALLBACK(on_mixer_revealed), self);

    // connect to request signals
    g_signal_connect(self, "quick-settings-req-open",
                     G_CALLBACK(on_quick_settings_req_open), NULL);
    g_signal_connect(self, "quick-settings-req-close",
                     G_CALLBACK(on_quick_settings_req_close), NULL);
    g_signal_connect(self, "quick-settings-req-shrink",
                     G_CALLBACK(on_quick_settings_req_shrink), NULL);
}

static void on_window_destroy(GtkWindow *win, QuickSettings *self) {
    g_debug("quick_settings.c:on_window_destroy() called.");
    quick_settings_reinitialize(self);
};

static void quick_settings_init(QuickSettings *self) {
    // initialize depedent widgets
    self->header = g_object_new(QUICK_SETTINGS_HEADER_TYPE, NULL);
    self->qs_grid = g_object_new(QUICK_SETTINGS_GRID_TYPE, NULL);

    quick_settings_init_layout(self);
};

static void animation_open_done(AdwAnimation *animation, QuickSettings *qs) {
    g_debug("quick_settings.c:animation_open_done() called.");

    // disconnect done signal
    g_signal_handlers_disconnect_by_func(qs->animation, animation_open_done,
                                         qs);
    // emit visible signal
    g_signal_emit(qs, signals[quick_settings_visible], 0);
};

void quick_settings_set_visible(QuickSettings *self) {
    int anim_state = 0;

    g_debug("quick_settings.c:quick_settings_set_visible() called.");

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
    g_signal_emit(self, signals[quick_settings_will_show], 0);

    // show underlay
    gtk_window_present(GTK_WINDOW(self->underlay));

    // present the window
    gtk_window_present(GTK_WINDOW(self->win));

    // connect animation done signal and play animation
    g_signal_connect(self->animation, "done", G_CALLBACK(animation_open_done),
                     self);
    adw_animation_play(self->animation);
}

QuickSettings *quick_settings_get_global() { return global; };

void quick_settings_reinitialize(QuickSettings *self) {
    g_debug("quick_settings.c:quick_settings_reinitialize() called.");

    // kill our signals
    g_signal_handlers_disconnect_by_func(self->win, on_window_destroy, self);
    g_signal_handlers_disconnect_by_func(self->header, on_mixer_revealed, self);

    // reinitialize dependent widgets
    quick_settings_grid_reinitialize(self->qs_grid);
    quick_settings_header_reinitialize(self->header);
    quick_settings_scales_reinitialize(self->scales);

    // reinit our layout
    quick_settings_init_layout(self);
}

void quick_settings_activate(AdwApplication *app, gpointer user_data) {
    global = g_object_new(QUICK_SETTINGS_TYPE, NULL);
};

void quick_settings_shrink(QuickSettings *qs) {
    g_debug("quick_settings.c:quick_settings_shrink() called.");
    gtk_window_set_default_size(GTK_WINDOW(qs->win), 400, 200);
};

void quick_settings_toggle(QuickSettings *qs) {
    g_debug("quick_settings.c:quick_settings_toggle() called.");
    if (quick_settings_is_visible(qs)) {
        quick_settings_set_hidden(qs);
    } else {
        quick_settings_set_visible(qs);
    }
};

void quick_settings_set_focused(QuickSettings *qs, gboolean focus) {
    gtk_widget_remove_css_class(GTK_WIDGET(qs->win), "focused");
    if (focus) {
        gtk_widget_add_css_class(GTK_WIDGET(qs->win), "focused");
    } else {
        gtk_widget_remove_css_class(GTK_WIDGET(qs->win), "focused");
    }
};

gboolean quick_settings_is_visible(QuickSettings *qs) {
    return gtk_widget_is_visible(GTK_WIDGET(qs->win));
};
