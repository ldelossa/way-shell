#include "quick_settings_grid_wifi_menu_option.h"

#include <NetworkManager.h>
#include <adwaita.h>

#include "../../../../services/network_manager_service.h"
#include "gtk/gtkrevealer.h"
#include "nm-dbus-interface.h"
#include "quick_settings_grid_wifi_menu.h"

typedef struct _QuickSettingsGridWifiMenuOption {
    GObject parent_instance;
    QuickSettingsGridWifiMenu *menu;
    NMDeviceWifi *dev;
    NMAccessPoint *ap;
    GtkBox *container;
    GtkButton *button;
    GtkImage *strength_icon;
    GtkImage *sec_icon;
    GtkLabel *ssid_label;
    GtkImage *active_icon;
    GtkRevealer *revealer;
    GtkBox *password_entry_container;
    GtkPasswordEntry *password_entry;
    GtkSpinner *spinner;
    gboolean has_sec;
} QuickSettingsGridWifiMenuOption;

// stub out the GObject interface.
G_DEFINE_TYPE(QuickSettingsGridWifiMenuOption,
              quick_settings_grid_wifi_menu_option, G_TYPE_OBJECT)

// signal callbacks
static void on_button_click_no_sec(GtkButton *button, gpointer user_data);

static void on_button_click_with_sec(GtkButton *button, gpointer user_data);

static void on_password_entry_activate(GtkPasswordEntry *entry,
                                       gpointer user_data);

static void on_menu_password_entry_revealed(
    QuickSettingsGridWifiMenu *menu, QuickSettingsGridWifiMenuOption *revealed,
    QuickSettingsGridWifiMenuOption *self);

static void quick_settings_grid_wifi_menu_option_dispose(GObject *self_) {
    QuickSettingsGridWifiMenuOption *self =
        QUICK_SETTINGS_GRID_WIFI_MENU_OPTION(self_);

    if (self->has_sec) {
        g_signal_handlers_disconnect_by_func(
            self->menu, on_menu_password_entry_revealed, self);
    };

    G_OBJECT_CLASS(quick_settings_grid_wifi_menu_option_parent_class)
        ->dispose(self_);
}

static void quick_settings_grid_wifi_menu_option_finalize(GObject *self_) {
    QuickSettingsGridWifiMenuOption *self =
        QUICK_SETTINGS_GRID_WIFI_MENU_OPTION(self_);

    G_OBJECT_CLASS(quick_settings_grid_wifi_menu_option_parent_class)
        ->finalize(self_);
}

static void quick_settings_grid_wifi_menu_option_class_init_layout(
    QuickSettingsGridWifiMenuOption *self) {
    // make container for option row
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->container),
                             "quick-settings-menu-option-wifi");

    // create spinner
    self->spinner = GTK_SPINNER(gtk_spinner_new());
    gtk_widget_set_visible(GTK_WIDGET(self->spinner), false);

    // make the main horizontal row box
    GtkBox *row = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

    // make password entry container
    self->password_entry_container =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_add_css_class(
        GTK_WIDGET(self->password_entry_container),
        "quick-settings-menu-option-wifi-password-entry-container");

    GtkImage *password_icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("dialog-password-symbolic"));
    gtk_box_append(self->password_entry_container, GTK_WIDGET(password_icon));

    self->password_entry = GTK_PASSWORD_ENTRY(gtk_password_entry_new());
    gtk_widget_set_hexpand(GTK_WIDGET(self->password_entry), TRUE);
    gtk_box_append(self->password_entry_container,
                   GTK_WIDGET(self->password_entry));
    gtk_password_entry_set_show_peek_icon(self->password_entry, TRUE);

    // make revealer for password entry
    self->revealer = GTK_REVEALER(gtk_revealer_new());
    gtk_revealer_set_transition_type(self->revealer,
                                     GTK_REVEALER_TRANSITION_TYPE_SWING_DOWN);
    gtk_revealer_set_transition_duration(self->revealer, 350);
    gtk_revealer_set_child(self->revealer,
                           GTK_WIDGET(self->password_entry_container));

    // make wifi icons
    self->strength_icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("network-wireless"));

    self->sec_icon = GTK_IMAGE(
        gtk_image_new_from_icon_name("network-wireless-encrypted-symbolic"));
    gtk_image_set_pixel_size(self->sec_icon, 10);
    gtk_widget_set_visible(GTK_WIDGET(self->sec_icon), false);

    self->active_icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("object-select-symbolic"));
    gtk_widget_set_visible(GTK_WIDGET(self->active_icon), false);

    // create SSID text label
    self->ssid_label = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(self->ssid_label, 0);
    gtk_label_set_ellipsize(self->ssid_label, PANGO_ELLIPSIZE_END);

    // wire it together
    gtk_box_append(row, GTK_WIDGET(self->strength_icon));
    gtk_box_append(row, GTK_WIDGET(self->sec_icon));
    gtk_box_append(row, GTK_WIDGET(self->ssid_label));
    gtk_box_append(row, GTK_WIDGET(self->active_icon));
    gtk_box_append(row, GTK_WIDGET(self->spinner));

    self->button = GTK_BUTTON(gtk_button_new());
    gtk_button_set_child(self->button, GTK_WIDGET(row));

    // append button to our containing box.
    gtk_box_append(self->container, GTK_WIDGET(self->button));
    // add revealer under button
    gtk_box_append(self->container, GTK_WIDGET(self->revealer));
}

static void on_button_click_no_sec(GtkButton *button, gpointer user_data) {
    QuickSettingsGridWifiMenuOption *self =
        QUICK_SETTINGS_GRID_WIFI_MENU_OPTION(user_data);

    g_debug("quick_settings_grid_wifi_menu_option.c:on_button_click_no_sec");

    // join specified ap
    NetworkManagerService *nm = network_manager_service_get_global();
    network_manager_service_ap_join(nm, self->dev, self->ap, NULL);
}

static void on_button_click_with_sec(GtkButton *button, gpointer user_data) {
    QuickSettingsGridWifiMenuOption *self =
        QUICK_SETTINGS_GRID_WIFI_MENU_OPTION(user_data);

    g_debug("quick_settings_grid_wifi_menu_option.c:on_button_click_with_sec");

    // show revealer
    gboolean revealed = gtk_revealer_get_reveal_child(self->revealer);
    if (revealed) {
        gtk_revealer_set_reveal_child(self->revealer, false);
    } else {
        gtk_revealer_set_reveal_child(self->revealer, true);
        gtk_widget_grab_focus(GTK_WIDGET(self->password_entry));
        // emit password revealed signal on behalf of menu
        g_signal_emit_by_name(self->menu, "password-entry-revealed", self);
    }
}

static void on_password_entry_activate(GtkPasswordEntry *entry,
                                       gpointer user_data) {
    QuickSettingsGridWifiMenuOption *self =
        QUICK_SETTINGS_GRID_WIFI_MENU_OPTION(user_data);

    g_debug(
        "quick_settings_grid_wifi_menu_option.c:on_password_entry_activate");

    const gchar *password;
    password =
        g_strdup(gtk_editable_get_text(GTK_EDITABLE(self->password_entry)));

    // debug password
    g_debug(
        "quick_settings_grid_wifi_menu_option.c:on_password_entry_activate: %s",
        password);

    // clear text field
    gtk_editable_delete_text(GTK_EDITABLE(self->password_entry), 0, -1);

    // we can reuse the on_click handler to close the password entry
    on_button_click_with_sec(NULL, self);

    // join specified AP with given password
    NetworkManagerService *nm = network_manager_service_get_global();
    network_manager_service_ap_join(nm, self->dev, self->ap, password);
}

void quick_settings_grid_wifi_menu_option_update_ap(
    QuickSettingsGridWifiMenuOption *self, NMDeviceWifi *dev,
    NMAccessPoint *ap) {
    NMDeviceState state = nm_device_get_state(NM_DEVICE(dev));
    gboolean is_active_ap = nm_device_wifi_get_active_access_point(dev) == ap;

    self->has_sec = false;
    if (nm_access_point_get_wpa_flags(ap) != NM_802_11_AP_SEC_NONE ||
        nm_access_point_get_rsn_flags(ap) != NM_802_11_AP_SEC_NONE)
        self->has_sec = true;

    if (is_active_ap) {
        // unhide spinner and start it
        gtk_widget_set_visible(GTK_WIDGET(self->spinner), true);
        gtk_spinner_start(self->spinner);
    }

    if (is_active_ap && state == NM_DEVICE_STATE_FAILED) {
        // stop spinner and hide it
        gtk_spinner_stop(self->spinner);
        gtk_widget_set_visible(GTK_WIDGET(self->spinner), false);
    }

    // update ssid label with ap name
    gtk_label_set_text(self->ssid_label,
                       network_manager_service_ap_to_name(ap));

    // update strength icon
    gtk_image_set_from_icon_name(
        self->strength_icon, network_manager_service_ap_strength_to_icon_name(
                                 nm_access_point_get_strength(ap)));

    // if password protected make self->sec_icon visible
    if (self->has_sec) {
        gtk_widget_set_visible(GTK_WIDGET(self->sec_icon), true);
    }

    // if active ap set active icon visible
    if (is_active_ap && state == NM_DEVICE_STATE_ACTIVATED) {
        gtk_widget_set_visible(GTK_WIDGET(self->active_icon), true);
        // hide spinner and stop it
        gtk_spinner_stop(self->spinner);
        gtk_widget_set_visible(GTK_WIDGET(self->spinner), false);
    } else
        gtk_widget_set_visible(GTK_WIDGET(self->active_icon), false);
}

static void on_menu_password_entry_revealed(
    QuickSettingsGridWifiMenu *menu, QuickSettingsGridWifiMenuOption *revealed,
    QuickSettingsGridWifiMenuOption *self) {
    if (revealed != self) gtk_revealer_set_reveal_child(self->revealer, false);
}

void quick_settings_grid_wifi_menu_option_set_ap(
    QuickSettingsGridWifiMenuOption *self, QuickSettingsGridWifiMenu *menu,
    NMDeviceWifi *dev, NMAccessPoint *ap) {
    NMAccessPoint *old_ap = self->ap;
    self->ap = ap;
    self->dev = dev;
    self->menu = menu;

    quick_settings_grid_wifi_menu_option_update_ap(self, dev, ap);

    // this was just a update of the existing AP.
    if (old_ap == self->ap) {
        return;
    }

    // add signals
    if (self->has_sec) {
        g_signal_connect(self->button, "clicked",
                         G_CALLBACK(on_button_click_with_sec), self);

        g_signal_connect(self->password_entry, "activate",
                         G_CALLBACK(on_password_entry_activate), self);
        // listen for menu to tell us other password entries have been revealed
        // to close ours
        g_signal_connect(menu, "password-entry-revealed",
                         G_CALLBACK(on_menu_password_entry_revealed), self);
    } else {
        g_signal_connect(self->button, "clicked",
                         G_CALLBACK(on_button_click_no_sec), self);
    }
}

GtkWidget *quick_settings_grid_wifi_menu_option_get_widget(
    QuickSettingsGridWifiMenuOption *self) {
    return GTK_WIDGET(self->container);
}

static void quick_settings_grid_wifi_menu_option_class_init(
    QuickSettingsGridWifiMenuOptionClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = quick_settings_grid_wifi_menu_option_dispose;
    object_class->finalize = quick_settings_grid_wifi_menu_option_finalize;
}

static void quick_settings_grid_wifi_menu_option_init(
    QuickSettingsGridWifiMenuOption *self) {
    quick_settings_grid_wifi_menu_option_class_init_layout(self);
}

// ap getter
NMAccessPoint *quick_settings_grid_wifi_menu_option_get_ap(
    QuickSettingsGridWifiMenuOption *self) {
    return self->ap;
}

// device getter
NMDeviceWifi *quick_settings_grid_wifi_menu_option_get_device(
    QuickSettingsGridWifiMenuOption *self) {
    return self->dev;
}
