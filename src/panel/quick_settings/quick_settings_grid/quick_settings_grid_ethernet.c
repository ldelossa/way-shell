#pragma once

#include "./quick_settings_grid_ethernet.h"

#include <adwaita.h>

#include "../../../services/network_manager_service.h"
#include "./quick_settings_grid_button.h"
#include "nm-dbus-interface.h"

static QuickSettingsGridEthernetMenu *qs_grid_ethernet_menu_init(
    QuickSettingsGridEthernetButton *button) {
    QuickSettingsGridEthernetMenu *self =
        g_malloc0(sizeof(QuickSettingsGridEthernetMenu));

    return self;
}

static void qs_grid_ethernet_menu_free(QuickSettingsGridEthernetMenu *self) {}

static void on_state_changed_ether(NMDeviceEthernet *dev, GParamSpec *pspec,
                                   QuickSettingsGridEthernetButton *button) {
    char *name = "";
    char *icon = "";
    NMDeviceState state = 0;

    state = nm_device_get_state(NM_DEVICE(dev));
    switch (state) {
        case NM_DEVICE_STATE_UNKNOWN:
        case NM_DEVICE_STATE_UNMANAGED:
        case NM_DEVICE_STATE_UNAVAILABLE:
        case NM_DEVICE_STATE_DISCONNECTED:
        case NM_DEVICE_STATE_FAILED:
        case NM_DEVICE_STATE_DEACTIVATING:
            icon = "network-wired-disconnected-symbolic";
            name = "no network";
            break;
        case NM_DEVICE_STATE_PREPARE:
        case NM_DEVICE_STATE_CONFIG:
        case NM_DEVICE_STATE_NEED_AUTH:
        case NM_DEVICE_STATE_IP_CONFIG:
        case NM_DEVICE_STATE_IP_CHECK:
        case NM_DEVICE_STATE_SECONDARIES:
            icon = "network-wired-acquiring-symbolic";
            name = "no internet";
            break;
        default:
            icon = "network-wired-symbolic";
            name = "connected";
            break;
    }

    // update self->subtitle label with ap name
    gtk_label_set_text(button->button.subtitle, name);
    // set ellipsize
    gtk_label_set_ellipsize(button->button.subtitle, PANGO_ELLIPSIZE_END);
    // set max characters 10
    gtk_label_set_max_width_chars(button->button.subtitle, 12);
    // update icon
    gtk_image_set_from_icon_name(button->button.icon, icon);
}

void static on_state_changed(NMDevice *dev, guint new_state, guint old_state,
                             guint reason,
                             QuickSettingsGridEthernetButton *self) {
    switch (reason) {
        case NM_DEVICE_STATE_REASON_NOW_UNMANAGED:
        case NM_DEVICE_STATE_REASON_REMOVED:
            g_signal_emit_by_name(self->button.cluster, "remove_button_req",
                                  self);
            // we are removed from cluster at this point, free ourselves
            quick_settings_grid_ethernet_button_free(self);
            return;
        default:
            break;
    }
    on_state_changed_ether(NM_DEVICE_ETHERNET(dev), NULL, self);
}

QuickSettingsGridEthernetButton *quick_settings_grid_ethernet_button_init(
    NMDeviceEthernet *dev) {
    QuickSettingsGridEthernetButton *self =
        g_malloc0(sizeof(QuickSettingsGridEthernetButton));

    quick_settings_grid_button_init(&self->button,
                                    QUICK_SETTINGS_BUTTON_ETHERNET, "Wired", "",
                                    "network-wired-symbolic", true);
    self->dev = dev;

    NMDeviceStateReason reason = nm_device_get_state_reason(NM_DEVICE(dev));
    on_state_changed(NM_DEVICE(dev), 0, 0, reason, self);

    // Monitor state changes for ethernet.
    g_signal_connect(dev, "state-changed", G_CALLBACK(on_state_changed), self);

    g_object_ref(dev);

    return self;
}

void quick_settings_grid_ethernet_button_free(
    QuickSettingsGridEthernetButton *self) {
    // kill signal handlers
    g_signal_handlers_disconnect_by_func(self->dev, on_state_changed, self);
    // unref button's container
    g_object_unref(self->button.container);
    // free ourselves
    g_free(self);
}