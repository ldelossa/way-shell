#include "quick_settings_header.h"

#include <adwaita.h>

#include "../quick_settings.h"
#include "../quick_settings_mediator.h"
#include "gtk/gtkrevealer.h"
#include "quick_settings_battery_button.h"
#include "quick_settings_header_mixer/quick_settings_header_mixer.h"
#include "quick_settings_power_menu.h"

enum signals { mixer_revealed, signals_n };

typedef struct _QuickSettingsHeader {
    GObject parent_instance;
    GtkBox *container;
    GtkCenterBox *center_box;
    QuickSettingsBatteryButton *battery_button;
    QuickSettingsPowerMenu *power_menu;
    GtkButton *power_button;
    GtkRevealer *power_menu_revealer;
    QuickSettingsHeaderMixer *mixer;
    GtkRevealer *mixer_revealer;
} QuickSettingsHeader;
guint signals[signals_n] = {0};
G_DEFINE_TYPE(QuickSettingsHeader, quick_settings_header, G_TYPE_OBJECT);

static void quick_settings_header_on_qs_hidden(QuickSettingsMediator *mediator,
                                               QuickSettings *qs,
                                               GdkMonitor *mon,
                                               QuickSettingsHeader *self) {
    g_debug(
        "quick_settings_header.c:quick_settings_header_on_qs_hidden() called.");
    gtk_revealer_set_reveal_child(self->power_menu_revealer, FALSE);
    gtk_revealer_set_reveal_child(self->mixer_revealer, FALSE);
}

static void on_power_button_click(GtkButton *button,
                                  QuickSettingsHeader *self) {
    g_debug("quick_settings.c:on_power_button_click() called.");
    QuickSettingsMediator *mediator = quick_settings_get_global_mediator();
    gboolean revealed =
        gtk_revealer_get_reveal_child(GTK_REVEALER(self->power_menu_revealer));
    if (!revealed) {
        quick_settings_mediator_req_set_focus(mediator, true);
        gtk_revealer_set_reveal_child(self->power_menu_revealer, true);
        gtk_revealer_set_reveal_child(self->mixer_revealer, FALSE);
    } else {
        quick_settings_mediator_req_set_focus(mediator, false);
        gtk_revealer_set_reveal_child(self->power_menu_revealer, false);
    }
};

static void on_mixer_button_click(GtkButton *button,
                                  QuickSettingsHeader *self) {
    g_debug("quick_settings.c:on_mixer_button_click() called.");
    QuickSettingsMediator *mediator = quick_settings_get_global_mediator();
    gboolean revealed =
        gtk_revealer_get_reveal_child(GTK_REVEALER(self->mixer_revealer));
    if (!revealed) {
        quick_settings_mediator_req_set_focus(mediator, true);
        gtk_revealer_set_reveal_child(self->mixer_revealer, true);
        gtk_revealer_set_reveal_child(self->power_menu_revealer, FALSE);
    } else {
        quick_settings_mediator_req_set_focus(mediator, false);
        gtk_revealer_set_reveal_child(self->mixer_revealer, false);
    }
}

// stub out empty dispose, finalize, class_init, and init methods for this
// GObject.
static void quick_settings_header_dispose(GObject *gobject) {
    g_debug("quick_settings_header.c:quick_settings_header_dispose() called.");

    QuickSettingsHeader *self = QUICK_SETTINGS_HEADER(gobject);

    // destroy our signals
    QuickSettingsMediator *mediator = quick_settings_get_global_mediator();
    g_signal_handlers_disconnect_by_func(
        self->power_button, G_CALLBACK(on_power_button_click), self);
    g_signal_handlers_disconnect_by_func(
        mediator, G_CALLBACK(quick_settings_header_on_qs_hidden), self);

    // Chain-up
    G_OBJECT_CLASS(quick_settings_header_parent_class)->dispose(gobject);
};

static void quick_settings_header_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(quick_settings_header_parent_class)->finalize(gobject);
};

static void quick_settings_header_class_init(QuickSettingsHeaderClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = quick_settings_header_dispose;
    object_class->finalize = quick_settings_header_finalize;

    signals[mixer_revealed] = g_signal_new(
        "mixer-revealed", G_TYPE_FROM_CLASS(object_class), G_SIGNAL_RUN_FIRST,
        0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
};

static void on_reveal_finish(GtkRevealer *revealer, GParamSpec *spec,
                             QuickSettingsHeader *self) {
    gboolean revealed =
        gtk_revealer_get_reveal_child(GTK_REVEALER(self->power_menu_revealer));
    QuickSettingsMediator *mediator = quick_settings_get_global_mediator();
    if (!revealed) quick_settings_mediator_req_shrink(mediator);
}

static void on_mixer_reveal_child(GtkRevealer *revealer, GParamSpec *spec,
                                  QuickSettingsHeader *self) {
    gboolean revealed =
        gtk_revealer_get_reveal_child(GTK_REVEALER(self->mixer_revealer));
    QuickSettingsMediator *mediator = quick_settings_get_global_mediator();
    if (!revealed) quick_settings_mediator_req_shrink(mediator);
    g_signal_emit(self, signals[mixer_revealed], 0, revealed);
}

static void quick_settings_header_init_layout(QuickSettingsHeader *self) {
    // create main layouts
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->container),
                        "quick-settings-header-container");
    self->center_box = GTK_CENTER_BOX(gtk_center_box_new());
    gtk_box_append(self->container, GTK_WIDGET(self->center_box));
    gtk_widget_set_name(GTK_WIDGET(self->center_box), "quick-settings-header");

    // create GtkRevealer to reveal power menu
    self->power_menu_revealer = GTK_REVEALER(gtk_revealer_new());
    gtk_revealer_set_transition_type(self->power_menu_revealer,
                                     GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(self->power_menu_revealer, 250);
    gtk_revealer_set_reveal_child(self->power_menu_revealer, FALSE);

    gtk_box_append(self->container, GTK_WIDGET(self->power_menu_revealer));

    g_signal_connect(self->power_menu_revealer, "notify::child-revealed",
                     G_CALLBACK(on_reveal_finish), self);

    gtk_revealer_set_child(
        self->power_menu_revealer,
        quick_settings_power_menu_get_widget(self->power_menu));

    // create GtkRevealer for mixer
    self->mixer_revealer = GTK_REVEALER(gtk_revealer_new());
    gtk_revealer_set_transition_type(self->mixer_revealer,
                                     GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(self->mixer_revealer, 250);
    gtk_revealer_set_reveal_child(self->mixer_revealer, FALSE);

    gtk_box_append(self->container, GTK_WIDGET(self->mixer_revealer));

    gtk_revealer_set_child(
        self->mixer_revealer,
        quick_settings_header_mixer_get_menu_widget(self->mixer));

    // create right side buttons box
    GtkBox *buttons_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8));

    // append mixer's button to button box
    gtk_box_append(
        buttons_box,
        GTK_WIDGET(quick_settings_header_mixer_get_mixer_button(self->mixer)));
    g_signal_connect(quick_settings_header_mixer_get_mixer_button(self->mixer),
                     "clicked", G_CALLBACK(on_mixer_button_click), self);

    // create power button and append it to buttons box
    self->power_button =
        GTK_BUTTON(gtk_button_new_from_icon_name("system-shutdown-symbolic"));
    gtk_widget_add_css_class(GTK_WIDGET(self->power_button), "circular");
    g_signal_connect(self->power_button, "clicked",
                     G_CALLBACK(on_power_button_click), self);

    gtk_box_append(buttons_box, GTK_WIDGET(self->power_button));

    gtk_center_box_set_end_widget(self->center_box, GTK_WIDGET(buttons_box));

    // add battery button to left side of header.
    gtk_center_box_set_start_widget(
        self->center_box,
        quick_settings_battery_button_get_widget(self->battery_button));

    // wire into quick settings mediator hidden event
    QuickSettingsMediator *mediator = quick_settings_get_global_mediator();
    g_signal_connect(mediator, "quick-settings-hidden",
                     G_CALLBACK(quick_settings_header_on_qs_hidden), self);

    // wire into mixer reavler's reveal child
    g_signal_connect(self->mixer_revealer, "notify::child-revealed",
                     G_CALLBACK(on_mixer_reveal_child), self);
}

static void quick_settings_header_init(QuickSettingsHeader *self) {
    // load up children depedencies.
    self->battery_button =
        g_object_new(QUICK_SETTINGS_BATTERY_BUTTON_TYPE, NULL);

    self->power_menu = g_object_new(QUICK_SETTINGS_POWER_MENU_TYPE, NULL);

    self->mixer = g_object_new(QUICK_SETTINGS_HEADER_MIXER_TYPE, NULL);

    // initialize layout
    quick_settings_header_init_layout(self);
}

void quick_settings_header_reinitialize(QuickSettingsHeader *self) {
    // destroy our signals
    QuickSettingsMediator *mediator = quick_settings_get_global_mediator();
    g_signal_handlers_disconnect_by_func(
        self->power_button, G_CALLBACK(on_power_button_click), self);
    g_signal_handlers_disconnect_by_func(
        mediator, G_CALLBACK(quick_settings_header_on_qs_hidden), self);

    // call reinitialize on children widget
    quick_settings_battery_button_reinitialize(self->battery_button);
    quick_settings_power_menu_reinitialize(self->power_menu);
    quick_settings_header_mixer_reinitialize(self->mixer);

    // call layout reinit for ourselves
    quick_settings_header_init_layout(self);
}

GtkWidget *quick_settings_header_get_widget(QuickSettingsHeader *self) {
    return GTK_WIDGET(self->container);
}
