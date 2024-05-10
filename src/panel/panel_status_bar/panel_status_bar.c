#include "panel_status_bar.h"

#include <adwaita.h>

#include "../panel.h"
#include "../quick_settings/quick_settings.h"
#include "./panel_status_bar_idle_inhibitor_button.h"
#include "./panel_status_bar_network_button.h"
#include "./panel_status_bar_power_button.h"
#include "./panel_status_bar_sound_button.h"

struct _PanelStatusBar {
    GObject parent_instance;
    GtkBox *container;
    GtkButton *button;
    GtkBox *box;
    Panel *panel;
    gboolean toggled;
    GPtrArray *buttons;
};
G_DEFINE_TYPE(PanelStatusBar, panel_status_bar, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init methods
static void panel_status_bar_dispose(GObject *gobject) {
    PanelStatusBar *self = PANEL_STATUS_BAR(gobject);

    // unref every GObject button in buttons array
    for (int i = 0; i < self->buttons->len; i++) {
        GObject *button = g_ptr_array_index(self->buttons, i);
        g_object_unref(button);
    }
    g_ptr_array_unref(self->buttons);

    // Chain-up
    G_OBJECT_CLASS(panel_status_bar_parent_class)->dispose(gobject);
};

static void panel_status_bar_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(panel_status_bar_parent_class)->finalize(gobject);
};

static void panel_status_bar_class_init(PanelStatusBarClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = panel_status_bar_dispose;
    object_class->finalize = panel_status_bar_finalize;
};

static void on_click(GtkButton *button, PanelStatusBar *self) {
    g_debug("quick_settings_button.c:on_click() called.");
    QuickSettings *qs = quick_settings_get_global();
    if (!qs) return;
    quick_settings_toggle(qs);
};

static void panel_status_bar_init_layout(PanelStatusBar *self) {
    self->toggled = false;

    self->buttons = g_ptr_array_new();

    // create main container
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

    // create button we will click to show quick settings UI.
    self->button = GTK_BUTTON(gtk_button_new());
    g_signal_connect(self->button, "clicked", G_CALLBACK(on_click), self);
    gtk_widget_add_css_class(GTK_WIDGET(self->button), "panel-button");

    // create holder for each button
    self->box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

    // create idle inhibitor button
    PanelStatusBarIdleInhibitorButton *idle_inhibitor_button =
        g_object_new(PANEL_STATUS_BAR_IDLE_INHIBITOR_BUTTON_TYPE, NULL);
    gtk_box_append(self->box, panel_status_bar_idle_inhibitor_button_get_widget(
                                  idle_inhibitor_button));
    // append to buttons
    g_ptr_array_add(self->buttons, idle_inhibitor_button);

    // create network icon and append it to box
    PanelStatusBarNetworkButton *network_button =
        g_object_new(PANEL_STATUS_BAR_NETWORK_BUTTON_TYPE, NULL);
    gtk_box_append(self->box,
                   panel_status_bar_network_button_get_widget(network_button));
    // append to buttons
    g_ptr_array_add(self->buttons, network_button);

    // create sound button and append it to box
    PanelStatusBarSoundButton *sound_button =
        g_object_new(PANEL_STATUS_BAR_SOUND_BUTTON_TYPE, NULL);
    gtk_box_append(self->box,
                   panel_status_bar_sound_button_get_widget(sound_button));
    // append to buttons
    g_ptr_array_add(self->buttons, sound_button);

    // create power button and append it to box
    PanelStatusBarPowerButton *power_button =
        g_object_new(PANEL_STATUS_BAR_POWER_BUTTON_TYPE, NULL);
    gtk_box_append(self->box,
                   panel_status_bar_power_button_get_widget(power_button));
    // append to buttons
    g_ptr_array_add(self->buttons, power_button);

    // attach box as child of button
    gtk_button_set_child(self->button, GTK_WIDGET(self->box));

    // attach button as child of container
    gtk_box_append(self->container, GTK_WIDGET(self->button));
}

static void panel_status_bar_init(PanelStatusBar *self) {
    panel_status_bar_init_layout(self);
}

GtkBox *panel_status_bar_get_widget(PanelStatusBar *self) {
    return self->container;
}

void panel_status_bar_set_panel(PanelStatusBar *self, Panel *panel) {
    self->panel = panel;
}

// Sets the QS button's toggle state.
// Effects the css applied to the button.
void panel_status_bar_set_toggled(PanelStatusBar *self, gboolean toggled) {
    self->toggled = toggled;
    if (toggled)
        gtk_widget_add_css_class(GTK_WIDGET(self->button),
                                 "panel-button-toggled");
    else
        gtk_widget_remove_css_class(GTK_WIDGET(self->button),
                                    "panel-button-toggled");
}

// Returns whether the QS button is toggled or not.
gboolean panel_status_bar_get_toggled(PanelStatusBar *self) {
    return self->toggled;
}
