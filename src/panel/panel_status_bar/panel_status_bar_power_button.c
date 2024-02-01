#include "panel_status_bar_power_button.h"

#include <adwaita.h>
#include <upower.h>

#include "../../services/upower_service.h"

struct _PanelStatusBarPowerButton {
    GObject parent_instance;
    UpDevice *power_dev;
    GtkImage *icon;
    int signals[2];
};
G_DEFINE_TYPE(PanelStatusBarPowerButton, panel_status_bar_power_button,
              G_TYPE_OBJECT);

static void on_battery_icon_change(GObject *object, GParamSpec *pspec,
                                   PanelStatusBarPowerButton *self) {
    char *icon_name = NULL;

    g_debug("panel_status_bar_power_button.c:on_battery_icon_change() called.");

    icon_name = upower_device_map_icon_name(UP_DEVICE(object));

    g_debug("quick_settings_button.c:on_battery_icon_change() icon_name: %s",
            icon_name);

    // update icon
    gtk_image_set_from_icon_name(self->icon, icon_name);
}

// stub out dispose, finalize, class_init, and init methods
static void panel_status_bar_power_button_dispose(GObject *gobject) {
    PanelStatusBarPowerButton *self = PANEL_STATUS_BAR_POWER_BUTTON(gobject);

    // disconnect from signals
    g_signal_handler_disconnect(self->power_dev, self->signals[0]);
    g_signal_handler_disconnect(self->power_dev, self->signals[1]);

    // unref power device
    g_object_unref(self->power_dev);

    // Chain-up
    G_OBJECT_CLASS(panel_status_bar_power_button_parent_class)
        ->dispose(gobject);
};

static void panel_status_bar_power_button_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(panel_status_bar_power_button_parent_class)
        ->finalize(gobject);
};

static void panel_status_bar_power_button_class_init(
    PanelStatusBarPowerButtonClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = panel_status_bar_power_button_dispose;
    object_class->finalize = panel_status_bar_power_button_finalize;
};

static void panel_status_bar_power_button_init_layout(
    PanelStatusBarPowerButton *self) {
    gchar *name = NULL;

    // get power device
    UPowerService *upower = upower_service_get_global();

    self->power_dev = upower_service_get_primary_device(upower);
    g_object_ref(self->power_dev);

    name = upower_device_map_icon_name(self->power_dev);

    // create icon
    self->icon = GTK_IMAGE(gtk_image_new_from_icon_name(name));

    self->signals[0] =
        g_signal_connect(self->power_dev, "notify::percentage",
                         G_CALLBACK(on_battery_icon_change), self);
    self->signals[1] =
        g_signal_connect(self->power_dev, "notify::state",
                         G_CALLBACK(on_battery_icon_change), self);

};

static void panel_status_bar_power_button_init(
    PanelStatusBarPowerButton *self) {
    panel_status_bar_power_button_init_layout(self);
}

GtkWidget *panel_status_bar_power_button_get_widget(
    PanelStatusBarPowerButton *self) {
    return GTK_WIDGET(self->icon);
}