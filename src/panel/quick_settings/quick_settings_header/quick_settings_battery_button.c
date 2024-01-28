#include "quick_settings_battery_button.h"

#include <adwaita.h>

#include "../../../services/upower_service.h"

typedef struct _QuickSettingsBatteryButton {
    GObject parent_instance;
    GtkBox *container;
    GtkImage *icon;
    GtkLabel *percentage;
    GtkButton *button;
    UpDevice *device;
} QuickSettingsBatteryButton;
G_DEFINE_TYPE(QuickSettingsBatteryButton, quick_settings_battery_button,
              G_TYPE_OBJECT);

static void on_power_dev_notify(UpDevice *power_dev, GParamSpec *pspec,
                                QuickSettingsBatteryButton *self) {
    char *icon = NULL;
    double percent = 0;

    g_debug("quick_settings_battery_button.c:on_power_dev_notify() called.");

    g_object_get(power_dev, "percentage", &percent, NULL);

    icon = upower_device_map_icon_name(power_dev);

    gtk_image_set_from_icon_name(self->icon, icon);

    g_debug("quick_settings_battery_button.c:on_power_dev_notify() icon: %s",
            icon);

    gtk_label_set_text(self->percentage, g_strdup_printf("%.0f%%", percent));
}

// stub out dispose, finalize, class_init and init methods.
static void quick_settings_battery_button_dispose(GObject *gobject) {
    QuickSettingsBatteryButton *self = QUICK_SETTINGS_BATTERY_BUTTON(gobject);

    g_debug(
        "quick_settings_battery_button.c:quick_settings_battery_button_dispose("
        ") called.");

    // disconnect from signals
    g_signal_handlers_disconnect_by_func(self->device,
                                         G_CALLBACK(on_power_dev_notify), self);

    // Chain-up
    G_OBJECT_CLASS(quick_settings_battery_button_parent_class)
        ->dispose(gobject);
};

static void quick_settings_battery_button_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(quick_settings_battery_button_parent_class)
        ->finalize(gobject);
};

static void quick_settings_battery_button_class_init(
    QuickSettingsBatteryButtonClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = quick_settings_battery_button_dispose;
    object_class->finalize = quick_settings_battery_button_finalize;
};

static void quick_settings_battery_button_init_layout(
    QuickSettingsBatteryButton *self) {
    UPowerService *upower = upower_service_get_global();
    char *icon = NULL;
    double percent = 0;

    // get primary power device
    UpDevice *power_dev = upower_service_get_primary_device(upower);

    icon = upower_device_map_icon_name(power_dev);

    g_object_get(power_dev, "percentage", &percent, NULL);

    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->container),
                        "quick-settings-header-bat-button");

    self->button = GTK_BUTTON(gtk_button_new());
    // add css class
    gtk_widget_add_css_class(GTK_WIDGET(self->button), "battery-button");

    self->icon = GTK_IMAGE(gtk_image_new_from_icon_name(icon));

    self->percentage =
        GTK_LABEL(gtk_label_new(g_strdup_printf("%.0f%%", percent)));

    // add button icon as first child of battery button container
    gtk_box_append(self->container, GTK_WIDGET(self->icon));

    // add percent label as next child
    gtk_box_append(self->container, GTK_WIDGET(self->percentage));

    // add container as child of battery button
    gtk_button_set_child(self->button, GTK_WIDGET(self->container));

    // setup notify signal
    g_signal_connect(power_dev, "notify::icon-name",
                     G_CALLBACK(on_power_dev_notify), self);
    g_signal_connect(power_dev, "notify::percentage",
                     G_CALLBACK(on_power_dev_notify), self);

    self->device = power_dev;
    g_object_ref(self->device);
}

static void quick_settings_battery_button_init(
    QuickSettingsBatteryButton *self) {
    quick_settings_battery_button_init_layout(self);
}

GtkWidget *quick_settings_battery_button_get_widget(
    QuickSettingsBatteryButton *self) {
    return GTK_WIDGET(self->button);
}

void quick_settings_battery_button_reinitialize(
    QuickSettingsBatteryButton *self) {
    // kill signals
    g_signal_handlers_disconnect_by_func(self->device,
                                         G_CALLBACK(on_power_dev_notify), self);
    // un ref powre device
    g_object_unref(self->device);

    // init our layout
    quick_settings_battery_button_init_layout(self);
}
