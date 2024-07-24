#include "panel_status_bar_airplane_mode_button.h"

#include <NetworkManager.h>
#include <adwaita.h>

#include "../../services/network_manager_service.h"
#include "glib.h"

struct _PanelStatusBarAirplaneModeButton {
    GObject parent_instance;
    GtkImage *icon;
};
G_DEFINE_TYPE(PanelStatusBarAirplaneModeButton,
              panel_status_bar_airplane_mode_button, G_TYPE_OBJECT);

static void handle_network_enabled_change(
    NetworkManagerService *nm, gboolean enabled,
    PanelStatusBarAirplaneModeButton *self) {
    // if we have an active airplane_mode connection show icon
    if (enabled)
        gtk_widget_set_visible(GTK_WIDGET(self->icon), false);
    else {
        gtk_widget_set_visible(GTK_WIDGET(self->icon), true);
    }
}

// stub out dispose, finalize, class_init, and init methods
static void panel_status_bar_airplane_mode_button_dispose(GObject *gobject) {
    PanelStatusBarAirplaneModeButton *self =
        PANEL_STATUS_BAR_AIRPLANE_MODE_BUTTON(gobject);

    // kill signals
    NetworkManagerService *nm = network_manager_service_get_global();
    g_signal_handlers_disconnect_by_func(nm, handle_network_enabled_change,
                                         self);

    // Chain-up
    G_OBJECT_CLASS(panel_status_bar_airplane_mode_button_parent_class)
        ->dispose(gobject);
};

static void panel_status_bar_airplane_mode_button_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(panel_status_bar_airplane_mode_button_parent_class)
        ->finalize(gobject);
};

static void panel_status_bar_airplane_mode_button_class_init(
    PanelStatusBarAirplaneModeButtonClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = panel_status_bar_airplane_mode_button_dispose;
    object_class->finalize = panel_status_bar_airplane_mode_button_finalize;
};

static void panel_status_bar_airplane_mode_button_init_layout(
    PanelStatusBarAirplaneModeButton *self) {
    self->icon = GTK_IMAGE(
        gtk_image_new_from_icon_name("airplane-mode-symbolic"));
    gtk_widget_set_visible(GTK_WIDGET(self->icon), false);

    // get initial airplane mode
    NetworkManagerService *nm = network_manager_service_get_global();
    gboolean enabled = network_manager_service_get_networking_enabled(nm);

    // if we have an active airplane_mode connection show icon
    if (!enabled) gtk_widget_set_visible(GTK_WIDGET(self->icon), true);

    // connect to airplane_mode-activated, airplane_mode-deactivated events
    g_signal_connect(nm, "networking-enabled-changed",
                     G_CALLBACK(handle_network_enabled_change), self);
};

static void panel_status_bar_airplane_mode_button_init(
    PanelStatusBarAirplaneModeButton *self) {
    panel_status_bar_airplane_mode_button_init_layout(self);
}

GtkWidget *panel_status_bar_airplane_mode_button_get_widget(
    PanelStatusBarAirplaneModeButton *self) {
    return GTK_WIDGET(self->icon);
}
