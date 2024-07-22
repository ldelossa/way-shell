#include "./quick_settings_grid_vpn.h"

#include <adwaita.h>

#include "../../../../services/network_manager_service.h"
#include "./../quick_settings_grid_button.h"
#include "glib.h"

static void on_toggle_button_clicked(GtkToggleButton *toggle_button,
                                     QuickSettingsGridVPNButton *self) {
    g_debug("quick_settings_grid_vpn.c:on_toggle_button_clicked() called");
    self->enabled = !self->enabled;
}

void quick_settings_grid_vpn_button_layout(QuickSettingsGridVPNButton *self) {
    // create associated menu
    self->menu = g_object_new(QUICK_SETTINGS_GRID_VPN_MENU_TYPE, NULL);

    quick_settings_grid_button_init(
        &self->button, QUICK_SETTINGS_BUTTON_VPN, "VPN", "",
        "network-vpn-disconnected-symbolic",
        quick_settings_grid_vpn_button_get_menu_widget(self),
        quick_settings_grid_vpn_menu_on_reveal);

    // write into toggle button's click event
    g_signal_connect(self->button.toggle, "clicked",
                     G_CALLBACK(on_toggle_button_clicked), self);
}

static void on_vpn_activated(NetworkManagerService *nm,
                             NMActiveConnection *vpn_conn,
                             QuickSettingsGridVPNButton *self) {
    g_debug("quick_settings_grid_vpn.c:on_vpn_activated() called");

    const gchar *id = nm_active_connection_get_id(vpn_conn);

    gtk_label_set_text(self->button.subtitle, id);
    gtk_image_set_from_icon_name(self->button.icon, "network-vpn-symbolic");

    quick_settings_grid_button_set_toggled(&self->button, true);
}

static void on_vpn_deactivated(NetworkManagerService *nm,
                               NMActiveConnection *vpn_conn,
                               QuickSettingsGridVPNButton *self) {
    g_debug("quick_settings_grid_vpn.c:on_vpn_deactivated() called");

    // check if we have any other active vpn connections.
    const char *active_conn = NULL;
    GHashTable *active_conns =
        network_manager_service_get_active_vpn_connections(nm);
    GList *values = g_hash_table_get_keys(active_conns);
    if (values) active_conn = values->data;

    gtk_label_set_text(self->button.subtitle, active_conn ?: "");
    gtk_image_set_from_icon_name(
        self->button.icon, active_conn ? "network-vpn-symbolic"
                                       : "network-vpn-disconnected-symbolic");

    quick_settings_grid_button_set_toggled(&self->button,
                                           active_conn ? true : false);
}

static void on_vpn_removed(NetworkManagerService *nm, NMConnection *vpn_conn,
                           int len, QuickSettingsGridVPNButton *self) {
    g_debug("quick_settings_grid_vpn.c:on_vpn_removed() called");
    if (len == 0)
        g_signal_emit_by_name(self->button.cluster, "remove_button_req", self);
}

QuickSettingsGridVPNButton *quick_settings_grid_vpn_button_init() {
    QuickSettingsGridVPNButton *self =
        g_malloc0(sizeof(QuickSettingsGridVPNButton));

    quick_settings_grid_vpn_button_layout(self);

    NetworkManagerService *nm = network_manager_service_get_global();
    g_signal_connect(nm, "vpn-removed", G_CALLBACK(on_vpn_removed), self);
    g_signal_connect(nm, "vpn-activated", G_CALLBACK(on_vpn_activated), self);
    g_signal_connect(nm, "vpn-deactivated", G_CALLBACK(on_vpn_deactivated),
                     self);

    quick_settings_grid_button_set_toggled(&self->button, false);

    GHashTable *active_conns =
        network_manager_service_get_active_vpn_connections(nm);
    GList *values = g_hash_table_get_values(active_conns);
    for (GList *l = values; l; l = l->next) {
        NMActiveConnection *vpn_conn = l->data;
        on_vpn_activated(nm, vpn_conn, self);
    }

    return self;
}

GtkWidget *quick_settings_grid_vpn_button_get_menu_widget(
    QuickSettingsGridVPNButton *self) {
    return GTK_WIDGET(quick_settings_grid_vpn_menu_get_widget(self->menu));
}

void quick_settings_grid_vpn_button_free(QuickSettingsGridVPNButton *self) {
    g_debug("quick_settings_grid_vpn.c:qs_grid_wifi_button_free() called");

    // kill network manager signals
    NetworkManagerService *nm = network_manager_service_get_global();
    g_signal_handlers_disconnect_by_func(nm, on_vpn_activated, self);
    g_signal_handlers_disconnect_by_func(nm, on_vpn_deactivated, self);

    // unref our menu
    g_object_unref(self->menu);
    // unref button's container
    g_object_unref(self->button.container);
    // free ourselves
    g_free(self);
}
