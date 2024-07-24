#include "panel_status_bar_vpn_button.h"

#include <NetworkManager.h>
#include <adwaita.h>

#include "../../services/network_manager_service.h"
#include "glib.h"

struct _PanelStatusBarVPNButton {
    GObject parent_instance;
    GtkImage *icon;
};
G_DEFINE_TYPE(PanelStatusBarVPNButton, panel_status_bar_vpn_button,
              G_TYPE_OBJECT);

static void handle_vpn_change(NetworkManagerService *nm,
                              NMActiveConnection *vpn_conn,
                              PanelStatusBarVPNButton *self) {

    GHashTable *active_conns =
        network_manager_service_get_active_vpn_connections(nm);
    // if we have an active vpn connection show icon
    if (active_conns && g_list_length(g_hash_table_get_keys(active_conns)) > 0)
        gtk_widget_set_visible(GTK_WIDGET(self->icon), true);
    else {
        gtk_widget_set_visible(GTK_WIDGET(self->icon), false);
    }
}

// stub out dispose, finalize, class_init, and init methods
static void panel_status_bar_vpn_button_dispose(GObject *gobject) {
    PanelStatusBarVPNButton *self = PANEL_STATUS_BAR_VPN_BUTTON(gobject);

    // kill signals
	NetworkManagerService *nm = network_manager_service_get_global();
	g_signal_handlers_disconnect_by_func(nm, handle_vpn_change, self);

    // Chain-up
    G_OBJECT_CLASS(panel_status_bar_vpn_button_parent_class)->dispose(gobject);
};

static void panel_status_bar_vpn_button_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(panel_status_bar_vpn_button_parent_class)->finalize(gobject);
};

static void panel_status_bar_vpn_button_class_init(
    PanelStatusBarVPNButtonClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = panel_status_bar_vpn_button_dispose;
    object_class->finalize = panel_status_bar_vpn_button_finalize;
};

static void panel_status_bar_vpn_button_init_layout(
    PanelStatusBarVPNButton *self) {
    self->icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("network-vpn-symbolic"));
    gtk_widget_set_visible(GTK_WIDGET(self->icon), false);

    // get initial active vpns
    NetworkManagerService *nm = network_manager_service_get_global();
    GHashTable *active_conns =
        network_manager_service_get_active_vpn_connections(nm);

    // if we have an active vpn connection show icon
    if (active_conns && g_list_length(g_hash_table_get_keys(active_conns)) > 0)
        gtk_widget_set_visible(GTK_WIDGET(self->icon), true);

    // connect to vpn-activated, vpn-deactivated events
    g_signal_connect(nm, "vpn-activated", G_CALLBACK(handle_vpn_change), self);
    g_signal_connect(nm, "vpn-deactivated", G_CALLBACK(handle_vpn_change),
                     self);
};

static void panel_status_bar_vpn_button_init(PanelStatusBarVPNButton *self) {
    panel_status_bar_vpn_button_init_layout(self);
}

GtkWidget *panel_status_bar_vpn_button_get_widget(
    PanelStatusBarVPNButton *self) {
    return GTK_WIDGET(self->icon);
}
