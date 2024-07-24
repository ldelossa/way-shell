#include "panel_status_bar_nightlight_button.h"

#include <NetworkManager.h>
#include <adwaita.h>

#include "../../services/wayland_service/wayland_service.h"

struct _PanelStatusBarNightLightButton {
    GObject parent_instance;
    GtkImage *icon;
};
G_DEFINE_TYPE(PanelStatusBarNightLightButton,
              panel_status_bar_nightlight_button, G_TYPE_OBJECT);

static void on_gamma_control_enabled(WaylandService *w,
                                     PanelStatusBarNightLightButton *self) {
    gtk_widget_set_visible(GTK_WIDGET(self->icon), true);
}

static void on_gamma_control_disabled(WaylandService *w,
                                      PanelStatusBarNightLightButton *self) {
    gtk_widget_set_visible(GTK_WIDGET(self->icon), false);
}

// stub out dispose, finalize, class_init, and init methods
static void panel_status_bar_nightlight_button_dispose(GObject *gobject) {
    PanelStatusBarNightLightButton *self =
        PANEL_STATUS_BAR_NIGHTLIGHT_BUTTON(gobject);

    // kill signals
    WaylandService *w = wayland_service_get_global();
    g_signal_handlers_disconnect_by_func(w, on_gamma_control_enabled, self);
    g_signal_handlers_disconnect_by_func(w, on_gamma_control_disabled, self);

    // Chain-up
    G_OBJECT_CLASS(panel_status_bar_nightlight_button_parent_class)
        ->dispose(gobject);
};

static void panel_status_bar_nightlight_button_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(panel_status_bar_nightlight_button_parent_class)
        ->finalize(gobject);
};

static void panel_status_bar_nightlight_button_class_init(
    PanelStatusBarNightLightButtonClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = panel_status_bar_nightlight_button_dispose;
    object_class->finalize = panel_status_bar_nightlight_button_finalize;
};

static void panel_status_bar_nightlight_button_init_layout(
    PanelStatusBarNightLightButton *self) {
    self->icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("night-light-symbolic"));
    gtk_widget_set_visible(GTK_WIDGET(self->icon), false);

    // get initial active nightlights
    WaylandService *w = wayland_service_get_global();
    if (wayland_wlr_gamma_control_enabled(w))
        gtk_widget_set_visible(GTK_WIDGET(self->icon), true);

    // if we have an active nightlight connection show icon
    g_signal_connect(w, "gamma-control-enabled",
                     G_CALLBACK(on_gamma_control_enabled), self);
    g_signal_connect(w, "gamma-control-disabled",
                     G_CALLBACK(on_gamma_control_disabled), self);
};

static void panel_status_bar_nightlight_button_init(
    PanelStatusBarNightLightButton *self) {
    panel_status_bar_nightlight_button_init_layout(self);
}

GtkWidget *panel_status_bar_nightlight_button_get_widget(
    PanelStatusBarNightLightButton *self) {
    return GTK_WIDGET(self->icon);
}
