#pragma once

#include "./quick_settings_grid_wifi.h"

#include <adwaita.h>

#include "./quick_settings_grid_button.h"
#include "nm-dbus-interface.h"

static QuickSettingsGridWifiMenu *qs_grid_wifi_menu_init(
    QuickSettingsGridWifiButton *button) {
    QuickSettingsGridWifiMenu *self =
        g_malloc0(sizeof(QuickSettingsGridWifiMenu));

    return self;
}

static void qs_grid_wifi_menu_free(QuickSettingsGridWifiMenu *self) {}

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
    NMDeviceState state = 0;

    g_debug("quick_settings_grid_wifi.c:on_active_access_point() called");

    state = nm_device_get_state(NM_DEVICE(dev));

    if (state != NM_DEVICE_STATE_ACTIVATED) {
        switch (state) {
            case NM_DEVICE_STATE_UNKNOWN:
            case NM_DEVICE_STATE_UNMANAGED:
            case NM_DEVICE_STATE_UNAVAILABLE:
            case NM_DEVICE_STATE_DISCONNECTED:
            case NM_DEVICE_STATE_FAILED:
            case NM_DEVICE_STATE_DEACTIVATING:
                icon = "network-wireless-offline-symbolic";
                break;
            case NM_DEVICE_STATE_PREPARE:
            case NM_DEVICE_STATE_CONFIG:
            case NM_DEVICE_STATE_NEED_AUTH:
            case NM_DEVICE_STATE_IP_CONFIG:
            case NM_DEVICE_STATE_IP_CHECK:
            case NM_DEVICE_STATE_SECONDARIES:
                break;
            default:
                break;
        }
    }

    NMAccessPoint *ap = nm_device_wifi_get_active_access_point(dev);
    if (ap) {
        GBytes *buff = nm_access_point_get_ssid(ap);
        if (buff) {
            guint8 size = g_bytes_get_size(buff);
            name = g_strdup(g_bytes_get_data(buff, NULL));
            // replace first occurrence of a non utf-8 character with null
            // byte.
            for (int i = 0; i < size; i++) {
                const char *c = &name[i];
                if (!g_utf8_validate(c, 1, NULL)) {
                    g_debug(
                        "quick_settings_grid_wifi.c:on_active_access_point() "
                        "found non utf-8 character");
                    name[i] = '\0';
                    break;
                }
            }
        }

        guint8 strength = nm_access_point_get_strength(ap);
        if (strength > 75) {
            icon = "network-wireless-signal-excellent-symbolic";
        } else if (strength > 50) {
            icon = "network-wireless-signal-good-symbolic";
        } else if (strength > 25) {
            icon = "network-wireless-signal-ok-symbolic";
        } else {
            icon = "network-wireless-signal-weak-symbolic";
        }
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

QuickSettingsGridWifiButton *quick_settings_grid_wifi_button_init(
    NMDeviceWifi *dev) {
    QuickSettingsGridWifiButton *self =
        g_malloc0(sizeof(QuickSettingsGridWifiButton));

    quick_settings_grid_button_init(&self->button, QUICK_SETTINGS_BUTTON_WIFI,
                                    "Wi-Fi", "", "network-wireless", true);
    self->dev = dev;

    on_active_access_point(dev, NULL, self);

    // connect to wifi device's notify::active-access-point
    g_signal_connect(dev, "notify::active-access-point",
                     G_CALLBACK(on_active_access_point), self);
    g_signal_connect(dev, "state-changed", G_CALLBACK(on_state_changed), self);

    g_object_ref(dev);

    return self;
}

void quick_settings_grid_wifi_button_free(QuickSettingsGridWifiButton *self) {
    g_debug("quick_settings_grid_wifi.c:qs_grid_wifi_button_free() called");
    // unref device
    g_object_unref(self->dev);
    // kill signal handlers
    g_signal_handlers_disconnect_by_func(self->dev, on_active_access_point,
                                         self);
    g_signal_handlers_disconnect_by_func(self->dev, on_state_changed, self);
    // unref button's container
    g_object_unref(self->button.container);
    // free ourselves
    g_free(self);
}