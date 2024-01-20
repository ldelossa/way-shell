#include "./quick_settings_grid_wifi.h"

#include <adwaita.h>

#include "../../../../services/network_manager_service.h"
#include "./../quick_settings_grid_button.h"
#include "nm-dbus-interface.h"

gboolean cleanup(QuickSettingsGridWifiButton *self, NMDevice *dev) {
    NMDeviceStateReason reason = nm_device_get_state_reason(dev);
    switch (reason) {
        case NM_DEVICE_STATE_REASON_AUTOIP_ERROR:
        case NM_DEVICE_STATE_REASON_NOW_UNMANAGED:
        case NM_DEVICE_STATE_REASON_REMOVED:
            g_signal_emit_by_name(self->button.cluster, "remove_button_req",
                                  self);
            // we are removed from cluster at this point, free ourselves
            quick_settings_grid_wifi_button_free(self);
            return true;
        default:
            break;
    }
    return false;
}

void static on_state_changed(NMDevice *dev, GParamSpec *pspec,
                             QuickSettingsGridWifiButton *self) {
    g_debug("quick_settings_grid_wifi.c:on_state_changed() called");
    cleanup(self, dev);
}

static void on_active_access_point(NMDeviceWifi *dev, GParamSpec *pspec,
                                   QuickSettingsGridWifiButton *self) {
    char *name = "";
    char *icon = "";
    gboolean preparing = false;
    NMDeviceState state = 0;

    on_state_changed(NM_DEVICE(dev), NULL, self);

    g_debug("quick_settings_grid_wifi.c:on_active_access_point() called");

    state = nm_device_get_state(NM_DEVICE(dev));

    switch (state) {
        case NM_DEVICE_STATE_UNKNOWN:
        case NM_DEVICE_STATE_UNMANAGED:
        case NM_DEVICE_STATE_UNAVAILABLE:
        case NM_DEVICE_STATE_DISCONNECTED:
        case NM_DEVICE_STATE_FAILED:
        case NM_DEVICE_STATE_DEACTIVATING:
            gtk_widget_add_css_class(GTK_WIDGET(self->button.toggle), "off");
            icon = "network-wireless-offline-symbolic";
            break;
        case NM_DEVICE_STATE_PREPARE:
        case NM_DEVICE_STATE_CONFIG:
        case NM_DEVICE_STATE_NEED_AUTH:
        case NM_DEVICE_STATE_IP_CONFIG:
        case NM_DEVICE_STATE_IP_CHECK:
        case NM_DEVICE_STATE_SECONDARIES:
            gtk_widget_add_css_class(GTK_WIDGET(self->button.toggle), "off");
            preparing = true;
            icon = "network-wireless-acquiring-symbolic";
            break;
        default:
            gtk_widget_remove_css_class(GTK_WIDGET(self->button.toggle), "off");
    }

    NMAccessPoint *ap = nm_device_wifi_get_active_access_point(dev);
    if (ap) {
        name = network_manager_service_ap_to_name(ap);
        guint8 strength = nm_access_point_get_strength(ap);
        if (!preparing)
            icon = network_manager_service_ap_strength_to_icon_name(strength);
    }

    // update self->subtitle label with ap name
    gtk_label_set_text(self->button.subtitle, name);
    // set ellipsize
    gtk_label_set_ellipsize(self->button.subtitle, PANGO_ELLIPSIZE_END);
    // set max characters 12
    gtk_label_set_max_width_chars(self->button.subtitle, 12);
    // update icon
    gtk_image_set_from_icon_name(self->button.icon, icon);
}

static void on_toggle_button_clicked(GtkToggleButton *toggle_button,
                                     QuickSettingsGridWifiButton *self) {
    g_debug("quick_settings_grid_wifi.c:on_toggle_button_clicked() called");
    if (self->enabled) {
        // turn off wifi
        network_manager_service_wireless_enable(
            network_manager_service_get_global(), false);
    } else {
        // turn on wifi
        network_manager_service_wireless_enable(
            network_manager_service_get_global(), true);
    }
    self->enabled = !self->enabled;
}

void quick_settings_grid_wifi_button_layout(QuickSettingsGridWifiButton *self,
                                            NMDeviceWifi *dev) {
    // create associated menu
    self->menu = g_object_new(QUICK_SETTINGS_GRID_WIFI_MENU_TYPE, NULL);
    quick_settings_grid_wifi_menu_set_device(self->menu, NM_DEVICE_WIFI(dev));

    quick_settings_grid_button_init(
        &self->button, QUICK_SETTINGS_BUTTON_WIFI, "Wi-Fi", "",
        "network-wireless",
        quick_settings_grid_wifi_button_get_menu_widget(self),
        quick_settings_grid_wifi_menu_on_reveal);

    self->dev = dev;

    on_active_access_point(dev, NULL, self);

    g_object_ref(dev);

    // connect notify to device's state change, this should capture all ap
    // events and device events as well like disconnnects.
    g_signal_connect(dev, "notify::state", G_CALLBACK(on_active_access_point),
                     self);

    // write into toggle button's click event
    g_signal_connect(self->button.toggle, "clicked",
                     G_CALLBACK(on_toggle_button_clicked), self);
}

QuickSettingsGridWifiButton *quick_settings_grid_wifi_button_init(
    NMDeviceWifi *dev) {
    QuickSettingsGridWifiButton *self =
        g_malloc0(sizeof(QuickSettingsGridWifiButton));

    // determine if device is enabled
    self->enabled = network_manager_service_wifi_available(
        network_manager_service_get_global());

    quick_settings_grid_wifi_button_layout(self, dev);

    return self;
}

GtkWidget *quick_settings_grid_wifi_button_get_menu_widget(
    QuickSettingsGridWifiButton *self) {
    return GTK_WIDGET(quick_settings_grid_wifi_menu_get_widget(self->menu));
}

void quick_settings_grid_wifi_button_free(QuickSettingsGridWifiButton *self) {
    g_debug("quick_settings_grid_wifi.c:qs_grid_wifi_button_free() called");
    // kill signal handlers
    g_signal_handlers_disconnect_by_func(self->dev, on_active_access_point,
                                         self);
    // unref device
    g_object_unref(self->dev);
    // unref our menu
    g_object_unref(self->menu);
    // unref button's container
    g_object_unref(self->button.container);
    // free ourselves
    g_free(self);
}