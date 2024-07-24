#include "quick_settings_grid_vpn_menu.h"

#include <NetworkManager.h>
#include <adwaita.h>

#include "../../../../services/network_manager_service.h"
#include "../../quick_settings_menu_widget.h"
#include "glib-object.h"
#include "gtk/gtk.h"
#include "nm-core-types.h"
#include "quick_settings_grid_vpn.h"

enum signals { signals_n };

typedef struct _QuickSettingsGridVPNMenu {
    GObject parent_instance;
    QuickSettingsMenuWidget menu;
    GHashTable *vpn_conns;
} QuickSettingsGridVPNMenu;
G_DEFINE_TYPE(QuickSettingsGridVPNMenu, quick_settings_grid_vpn_menu,
              G_TYPE_OBJECT);

// foward declarations for network manager signal handlers
static void on_vpn_added(NetworkManagerService *nm, NMConnection *vpn_conn,
                         int len, QuickSettingsGridVPNMenu *self);
static void on_vpn_removed(NetworkManagerService *nm, NMConnection *vpn_conn,
                           int len, QuickSettingsGridVPNMenu *self);
static void on_vpn_activated(NetworkManagerService *nm,
                             NMActiveConnection *vpn_conn,
                             QuickSettingsGridVPNMenu *self);
static void on_vpn_deactivated(NetworkManagerService *nm,
                               NMActiveConnection *vpn_conn,
                               QuickSettingsGridVPNMenu *self);

static void on_networking_enabled(NetworkManagerService *nm, gboolean enabled,
                                  QuickSettingsGridVPNMenu *self) {
    GtkWidget *child =
        gtk_widget_get_first_child(GTK_WIDGET(self->menu.options));

    if (enabled) {
        while (child) {
            gtk_widget_set_sensitive(child, true);
            gtk_widget_set_focusable(child, true);
            child = gtk_widget_get_next_sibling(child);
        }
    } else {
        while (child) {
            gtk_widget_set_sensitive(child, false);
            gtk_widget_set_focusable(child, false);
            child = gtk_widget_get_next_sibling(child);
        }
    }
}

// stub out dispose, finalize, init and class init functions for GObject
static void quick_settings_grid_vpn_menu_dispose(GObject *object) {
    QuickSettingsGridVPNMenu *self = QUICK_SETTINGS_GRID_VPN_MENU(object);

    NetworkManagerService *nm = network_manager_service_get_global();

    // disconnect from signals
    g_signal_handlers_disconnect_by_func(nm, on_networking_enabled, self);
    g_signal_handlers_disconnect_by_func(nm, on_vpn_added, self);
    g_signal_handlers_disconnect_by_func(nm, on_vpn_removed, self);
    g_signal_handlers_disconnect_by_func(nm, on_vpn_activated, self);
    g_signal_handlers_disconnect_by_func(nm, on_vpn_deactivated, self);

    // free vpn_conns hash table
    g_hash_table_destroy(self->vpn_conns);

    G_OBJECT_CLASS(quick_settings_grid_vpn_menu_parent_class)->dispose(object);
}

static void quick_settings_grid_vpn_menu_finalize(GObject *object) {}

static void quick_settings_grid_vpn_menu_class_init(
    QuickSettingsGridVPNMenuClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = quick_settings_grid_vpn_menu_dispose;
    object_class->finalize = quick_settings_grid_vpn_menu_finalize;
}

static void on_vpn_conn_widget_activate(AdwSwitchRow *row, GParamSpec *spec,
                                        QuickSettingsGridVPNMenu *self) {
    // activating the switch is a trigger to connect to the VPN.
    // Therefore, when we see the switch 'activated' we immediately
    // deactivate it and then attempt to connect to the vpn associated with
    // the id.
    const gchar *id = adw_preferences_row_get_title(ADW_PREFERENCES_ROW(row));
    gboolean activated = adw_switch_row_get_active(row);

    NetworkManagerService *nm = network_manager_service_get_global();

    if (activated) {
        network_manager_activate_vpn(nm, id, true);
    } else {
        network_manager_activate_vpn(nm, id, false);
    }
}

GtkWidget *new_vpn_conn_row(NMConnection *vpn_con,
                            QuickSettingsGridVPNMenu *self) {
    GtkWidget *row = adw_switch_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row),
                                  nm_connection_get_id(vpn_con));
    gtk_widget_add_css_class(GTK_WIDGET(row), "switch-row-vpn");

    g_signal_connect(row, "notify::active",
                     G_CALLBACK(on_vpn_conn_widget_activate), self);

    return row;
}

static void on_vpn_added(NetworkManagerService *nm, NMConnection *vpn_conn,
                         int len, QuickSettingsGridVPNMenu *self) {
    // add a new VPN Switch Row Widget
    GtkWidget *row = new_vpn_conn_row(vpn_conn, self);

    g_hash_table_insert(self->vpn_conns,
                        g_strdup(nm_connection_get_id(vpn_conn)), row);

    gtk_box_append(self->menu.options, row);
}

static void on_vpn_removed(NetworkManagerService *nm, NMConnection *vpn_conn,
                           int len, QuickSettingsGridVPNMenu *self) {
    // remove the VPN Swithc Row Widget if it exists
    GtkWidget *row =
        g_hash_table_lookup(self->vpn_conns, nm_connection_get_id(vpn_conn));
    if (row) {
        gtk_box_remove(self->menu.options, row);
        g_hash_table_remove(self->vpn_conns, nm_connection_get_id(vpn_conn));
    }
}

static void on_vpn_activated(NetworkManagerService *nm,
                             NMActiveConnection *vpn_conn,
                             QuickSettingsGridVPNMenu *self) {
    // find row widget by connection id
    GtkWidget *row = g_hash_table_lookup(self->vpn_conns,
                                         nm_active_connection_get_id(vpn_conn));
    if (!row) return;

    g_signal_handlers_block_by_func(row, on_vpn_conn_widget_activate, self);
    adw_switch_row_set_active(ADW_SWITCH_ROW(row), TRUE);
    g_signal_handlers_unblock_by_func(row, on_vpn_conn_widget_activate, self);
}

static void on_vpn_deactivated(NetworkManagerService *nm,
                               NMActiveConnection *vpn_conn,
                               QuickSettingsGridVPNMenu *self) {
    // find row widget by connection id
    GtkWidget *row = g_hash_table_lookup(self->vpn_conns,
                                         nm_active_connection_get_id(vpn_conn));
    if (!row) return;

    // receiving event so block notify::active event from firing
    g_signal_handlers_block_by_func(row, on_vpn_conn_widget_activate, self);
    adw_switch_row_set_active(ADW_SWITCH_ROW(row), FALSE);
    g_signal_handlers_unblock_by_func(row, on_vpn_conn_widget_activate, self);
}

static void quick_settings_grid_vpn_menu_init_layout(
    QuickSettingsGridVPNMenu *self) {
    quick_settings_menu_widget_init(&self->menu, false);
    quick_settings_menu_widget_set_icon(&self->menu, "network-vpn-symbolic");
    quick_settings_menu_widget_set_title(&self->menu, "VPN");

    NetworkManagerService *nm = network_manager_service_get_global();

    // Populate current VPN Networks
    if (!network_manager_has_vpn(nm)) return;

    GHashTable *vpn_conns = network_manager_get_vpn_connections(nm);
    GList *values = g_hash_table_get_values(vpn_conns);
    for (GList *l = values; l; l = l->next) {
        NMConnection *vpn_conn = l->data;
        on_vpn_added(nm, vpn_conn, 0, self);
    }

    GHashTable *active_vpn_conns =
        network_manager_service_get_active_vpn_connections(nm);
    GList *active_values = g_hash_table_get_values(active_vpn_conns);
    for (GList *l = active_values; l; l = l->next) {
        NMActiveConnection *vpn_conn = l->data;
        on_vpn_activated(nm, vpn_conn, self);
    }

    g_signal_connect(nm, "networking-enabled-changed",
                     G_CALLBACK(on_networking_enabled), self);

    g_signal_connect(nm, "vpn-added", G_CALLBACK(on_vpn_added), self);
    g_signal_connect(nm, "vpn-removed", G_CALLBACK(on_vpn_removed), self);
    g_signal_connect(nm, "vpn-activated", G_CALLBACK(on_vpn_activated), self);
    g_signal_connect(nm, "vpn-deactivated", G_CALLBACK(on_vpn_deactivated),
                     self);
}

static void quick_settings_grid_vpn_menu_init(QuickSettingsGridVPNMenu *self) {
    self->vpn_conns =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    quick_settings_grid_vpn_menu_init_layout(self);
}

void quick_settings_grid_vpn_menu_on_reveal(QuickSettingsGridButton *button_,
                                            gboolean is_revealed) {
    QuickSettingsGridVPNButton *button = (QuickSettingsGridVPNButton *)button_;
    QuickSettingsGridVPNMenu *self = button->menu;

    if (!is_revealed) {
    }

    g_debug("quick_settings_grid_vpn_menu.c:on_reveal() called");
}

GtkWidget *quick_settings_grid_vpn_menu_get_widget(
    QuickSettingsGridVPNMenu *self) {
    return GTK_WIDGET(self->menu.container);
}
