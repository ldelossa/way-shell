#include "panel_status_bar_network_button.h"

#include <adwaita.h>

#include "../../services/network_manager_service.h"

struct _PanelStatusBarNetworkButton {
    GObject parent_instance;
    GtkImage *icon;
    guint32 signal_id;
};
G_DEFINE_TYPE(PanelStatusBarNetworkButton, panel_status_bar_network_button,
              G_TYPE_OBJECT);

static void on_nm_change(NetworkManagerService *nm,
                         PanelStatusBarNetworkButton *self) {
    gboolean has_wifi = network_manager_service_wifi_available(nm);
    NMDevice *dev = network_manager_service_get_primary_device(nm);
    NMState state = network_manager_service_get_state(nm);

    g_debug("panel_status_bar_network_button.c:on_nm_change() called.");

    // check if network is disabled, if it is, we hide the wifi button all
	// together.
    gboolean enabled = network_manager_service_get_networking_enabled(nm);
    if (!enabled) {
        // hide icon and return
        gtk_widget_set_visible(GTK_WIDGET(self->icon), false);
		return;
    } else {
        // show icon
        gtk_widget_set_visible(GTK_WIDGET(self->icon), true);
    }

    NMDeviceType type = 0;
    if (dev) type = nm_device_get_device_type(dev);

    switch (state) {
        case NM_STATE_UNKNOWN:
        case NM_STATE_ASLEEP:
        case NM_STATE_DISCONNECTING:
        case NM_STATE_DISCONNECTED:
            if (has_wifi) {
                gtk_image_set_from_icon_name(
                    self->icon, "network-wireless-offline-symbolic");
                return;
            }
            gtk_image_set_from_icon_name(self->icon,
                                         "network-wired-offline-symbolic");
            return;
        case NM_STATE_CONNECTING:
            if (type == NM_DEVICE_TYPE_WIFI) {
                gtk_image_set_from_icon_name(
                    self->icon, "network-wireless-acquiring-symbolic");
                return;
            }
            gtk_image_set_from_icon_name(self->icon,
                                         "network-wired-acquiring-symbolic");
            return;
        case NM_STATE_CONNECTED_LOCAL:
        case NM_STATE_CONNECTED_SITE:
            // LOCAL/SITE connectivity can occur when a wifi device is
            // disconnected but any other network devices (even linux bridges)
            // remain connected to a network.
            //
            // therefore if we see this state and we also have wifi adapters,
            // confirm whether the adapters are disconnected or not.
            if (has_wifi) {
                NMDeviceState state = network_manager_service_wifi_state(nm);
                if (state == NM_DEVICE_STATE_DISCONNECTED) {
                    gtk_image_set_from_icon_name(
                        self->icon, "network-wireless-offline-symbolic");
                    return;
                }
                gtk_image_set_from_icon_name(
                    self->icon, "network-wireless-no-route-symbolic");
                return;
            }
            gtk_image_set_from_icon_name(self->icon,
                                         "network-wired-no-route-symbolic");
            return;
        case NM_STATE_CONNECTED_GLOBAL:
            if (type == NM_DEVICE_TYPE_WIFI) {
                NMDeviceWifi *wifi = NM_DEVICE_WIFI(dev);
                NMAccessPoint *ap =
                    nm_device_wifi_get_active_access_point(wifi);
                if (!ap) {
                    return;
                }
                guint8 strength = nm_access_point_get_strength(ap);
                char *icon_name =
                    network_manager_service_ap_strength_to_icon_name(strength);
                gtk_image_set_from_icon_name(self->icon, icon_name);
                return;
            }
            gtk_image_set_from_icon_name(self->icon, "network-wired-symbolic");
            return;
    }
};

// stub out dispose, finalize, class_init, and init methods
static void panel_status_bar_network_button_dispose(GObject *gobject) {
    PanelStatusBarNetworkButton *self =
        PANEL_STATUS_BAR_NETWORK_BUTTON(gobject);

    // disconnect from signals
    NetworkManagerService *nm = network_manager_service_get_global();
    g_signal_handler_disconnect(nm, self->signal_id);

    // Chain-up
    G_OBJECT_CLASS(panel_status_bar_network_button_parent_class)
        ->dispose(gobject);
};

static void panel_status_bar_network_button_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(panel_status_bar_network_button_parent_class)
        ->finalize(gobject);
};

static void panel_status_bar_network_button_class_init(
    PanelStatusBarNetworkButtonClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = panel_status_bar_network_button_dispose;
    object_class->finalize = panel_status_bar_network_button_finalize;
};

static void panel_status_bar_network_button_init_layout(
    PanelStatusBarNetworkButton *self) {
    NetworkManagerService *nm = network_manager_service_get_global();

    self->icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("network-wired-symbolic"));

    on_nm_change(nm, self);
    self->signal_id =
        g_signal_connect(nm, "changed", G_CALLBACK(on_nm_change), self);
};

static void panel_status_bar_network_button_init(
    PanelStatusBarNetworkButton *self) {
    panel_status_bar_network_button_init_layout(self);
}

GtkWidget *panel_status_bar_network_button_get_widget(
    PanelStatusBarNetworkButton *self) {
    return GTK_WIDGET(self->icon);
}
