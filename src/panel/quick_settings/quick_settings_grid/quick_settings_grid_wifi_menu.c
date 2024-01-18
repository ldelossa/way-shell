#include "quick_settings_grid_wifi_menu.h"

#include <NetworkManager.h>
#include <adwaita.h>

#include "../../../services/network_manager_service.h"
#include "../quick_settings_menu_widget.h"
#include "quick_settings_grid_wifi.h"

typedef struct _QuickSettingsGridWifiMenu {
    QuickSettingsMenuWidget menu;
    NMDeviceWifi *dev;
    GCancellable *cancel_scan;
    GHashTable *access_points;
    gint64 last_scan;
    GtkSpinner *spinner;
} QuickSettingsGridWifiMenu;
G_DEFINE_TYPE(QuickSettingsGridWifiMenu, quick_settings_grid_wifi_menu,
              G_TYPE_OBJECT);

// stub out dispose, finalize, init and class init functions for GObject
static void quick_settings_grid_wifi_menu_dispose(GObject *object) {
    QuickSettingsGridWifiMenu *self = QUICK_SETTINGS_GRID_WIFI_MENU(object);
}

static void quick_settings_grid_wifi_menu_finalize(GObject *object) {
    QuickSettingsGridWifiMenu *self = QUICK_SETTINGS_GRID_WIFI_MENU(object);
}

static void quick_settings_grid_wifi_menu_class_init(
    QuickSettingsGridWifiMenuClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = quick_settings_grid_wifi_menu_dispose;
    object_class->finalize = quick_settings_grid_wifi_menu_finalize;
}

typedef struct _AcessPointMenuOption {
    GtkBox *container;
    GtkBox *row;
    GtkButton *button;
    GtkImage *icon;
    GtkImage *sec_icon;
    GtkLabel *ssid_label;
    GtkImage *active_icon;
} AccessPointMenuOption;

AccessPointMenuOption *access_point_option_init(NMAccessPoint *ap,
                                                gboolean is_active) {
    AccessPointMenuOption *self = g_malloc0(sizeof(AccessPointMenuOption));

    // make container for option row
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->container),
                             "quick-settings-menu-option-wifi");

    // make the main horizontal row box
    GtkBox *row = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

    // map icon name from AP's strength.
    char *icon_name = network_manager_service_ap_strength_to_icon_name(
        nm_access_point_get_strength(ap));

    // Make icons
    self->icon = GTK_IMAGE(gtk_image_new_from_icon_name(icon_name));
    self->sec_icon = GTK_IMAGE(
        gtk_image_new_from_icon_name("network-wireless-encrypted-symbolic"));
    gtk_image_set_pixel_size(self->sec_icon, 10);
    self->active_icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("object-select-symbolic"));

    // check if this is a secure network, if so append a encrypted icon.
    if (nm_access_point_get_wpa_flags(ap) == NM_802_11_AP_SEC_NONE) {
        gtk_widget_set_visible(GTK_WIDGET(self->sec_icon), FALSE);
    }

    // create SSID text label
    self->ssid_label =
        GTK_LABEL(gtk_label_new(network_manager_service_ap_to_name(ap)));
    gtk_label_set_xalign(self->ssid_label, 0);
    gtk_label_set_ellipsize(self->ssid_label, PANGO_ELLIPSIZE_END);

    // if this is not the active AP make selected mark at end hidden
    if (!is_active) {
        gtk_widget_set_visible(GTK_WIDGET(self->active_icon), FALSE);
    }

    // wire it together

    gtk_box_append(row, GTK_WIDGET(self->icon));
    gtk_box_append(row, GTK_WIDGET(self->sec_icon));
    gtk_box_append(row, GTK_WIDGET(self->ssid_label));
    gtk_box_append(row, GTK_WIDGET(self->active_icon));

    self->button = GTK_BUTTON(gtk_button_new());
    gtk_button_set_child(self->button, GTK_WIDGET(row));

    // append button to our containing box.
    gtk_box_append(self->container, GTK_WIDGET(self->button));

    return self;
}

static void wifi_device_get_aps(NMDeviceWifi *wifi, GParamSpec *pspec,
                                QuickSettingsGridWifiMenu *self) {
    const GPtrArray *aps = nm_device_wifi_get_access_points(wifi);
    NMAccessPoint *active = nm_device_wifi_get_active_access_point(wifi);
    GHashTable *set = g_hash_table_new(g_direct_hash, g_direct_equal);

    g_debug("quick_settings_grid_wifi_menu.c:wifi_device_get_aps() called");

    // check if we have a pointer to this ap in our hash table
    // if not, add it.
    for (int i = 0; i < aps->len; i++) {
        NMAccessPoint *ap = g_ptr_array_index(aps, i);
        g_hash_table_add(set, ap);

        if (!g_hash_table_contains(self->access_points, ap)) {
            AccessPointMenuOption *opt =
                access_point_option_init(ap, ap == active);

            gtk_box_append(self->menu.options, GTK_WIDGET(opt->container));

            g_hash_table_insert(self->access_points, ap, opt);
        } else {
            AccessPointMenuOption *opt =
                g_hash_table_lookup(self->access_points, ap);

            // update strength icon
            char *icon_name = network_manager_service_ap_strength_to_icon_name(
                nm_access_point_get_strength(ap));
            gtk_image_set_from_icon_name(opt->icon, icon_name);

            // update active ap icon
            if (ap == active)
                gtk_widget_set_visible(GTK_WIDGET(opt->active_icon), TRUE);
            else
                gtk_widget_set_visible(GTK_WIDGET(opt->active_icon), FALSE);
        }
    }

    // remove stale access points in self->access_points
    GHashTableIter iter;
    NMAccessPoint *key;
    AccessPointMenuOption *value;
    g_hash_table_iter_init(&iter, self->access_points);
    while (
        g_hash_table_iter_next(&iter, (gpointer *)&key, (gpointer *)&value)) {
        if (!g_hash_table_contains(set, key)) {
            g_hash_table_iter_remove(&iter);
            gtk_widget_unparent(GTK_WIDGET(value->container));
        }
    }
}

static void quick_settings_grid_wifi_menu_init_layout(
    QuickSettingsGridWifiMenu *self) {
    quick_settings_menu_widget_init(&self->menu, true);
    quick_settings_menu_widget_set_icon(
        &self->menu, "network-wireless-signal-excellent-symbolic");
    quick_settings_menu_widget_set_title(&self->menu, "Wi-Fi");

    // add spinner to title_container
    self->spinner = GTK_SPINNER(gtk_spinner_new());
    gtk_box_append(self->menu.title_container, GTK_WIDGET(self->spinner));
    gtk_widget_set_halign(GTK_WIDGET(self->spinner), GTK_ALIGN_END);
}

static void quick_settings_grid_wifi_menu_init(
    QuickSettingsGridWifiMenu *self) {
    self->cancel_scan = g_cancellable_new();
    // create a pointer to pointer hash table
    self->access_points = g_hash_table_new(g_direct_hash, g_direct_equal);
    quick_settings_grid_wifi_menu_init_layout(self);
}

void quick_settings_grid_wifi_menu_set_device(QuickSettingsGridWifiMenu *self,
                                              NMDeviceWifi *dev) {
    self->dev = dev;
    wifi_device_get_aps(dev, NULL, self);

    // wire into active access point change
    g_signal_connect(self->dev, "notify::active-access-point",
                     G_CALLBACK(wifi_device_get_aps), self);
}

void quick_settings_grid_wifi_menu_on_scan_done(GObject *object,
                                                GAsyncResult *res,
                                                void *self_) {
    QuickSettingsGridWifiMenu *self = (QuickSettingsGridWifiMenu *)self_;
    NMDeviceWifi *dev = NM_DEVICE_WIFI(object);

    g_debug("quick_settings_grid_wifi_menu.c:on_scan_done() called");

    GError *error = NULL;
    if (!nm_device_wifi_request_scan_finish(dev, res, &error)) {
        g_warning(
            "quick_settings_grid_wifi_menu.c:on_scan_done(): error scanning "
            "for wifi device %s",
            error->message);
        g_clear_error(&error);
    } else {
        wifi_device_get_aps(dev, NULL, self);
    }
    gtk_spinner_stop(self->spinner);
}

void quick_settings_grid_wifi_menu_on_reveal(QuickSettingsGridButton *button_,
                                             gboolean is_revealed) {
    QuickSettingsGridWifiButton *button =
        (QuickSettingsGridWifiButton *)button_;
    QuickSettingsGridWifiMenu *self = button->menu;

    if (!is_revealed) {
    }

    g_debug("quick_settings_grid_wifi_menu.c:on_reveal() called");

    if (!self->dev) return;

    g_debug(
        "quick_settings_grid_wifi_menu.c:on_reveal() scanning wifi network...");
    gtk_spinner_start(self->spinner);

    // start an async scan
    nm_device_wifi_request_scan_async(
        self->dev, self->cancel_scan,
        quick_settings_grid_wifi_menu_on_scan_done, self);
}

GtkWidget *quick_settings_grid_wifi_menu_get_widget(
    QuickSettingsGridWifiMenu *self) {
    return GTK_WIDGET(self->menu.container);
}