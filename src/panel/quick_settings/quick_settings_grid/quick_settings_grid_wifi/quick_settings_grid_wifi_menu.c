#include "quick_settings_grid_wifi_menu.h"

#include <NetworkManager.h>
#include <adwaita.h>

#include "../../quick_settings.h"
#include "../../quick_settings_menu_widget.h"
#include "nm-dbus-interface.h"
#include "quick_settings_grid_wifi.h"
#include "quick_settings_grid_wifi_menu_option.h"

enum signals { password_entry_revealed, signals_n };

typedef struct _QuickSettingsGridWifiMenu {
    GObject parent_instance;
    QuickSettingsMenuWidget menu;
    GPtrArray *ap_options;
    NMDeviceWifi *dev;
    GCancellable *cancel_scan;
    gint64 last_scan;
    GtkSpinner *spinner;
    GtkBox *failure_banner_container;
    GtkLabel *failure_banner_label;
    GtkButton *failure_banner_dismiss_button;
} QuickSettingsGridWifiMenu;
G_DEFINE_TYPE(QuickSettingsGridWifiMenu, quick_settings_grid_wifi_menu,
              G_TYPE_OBJECT);

static void wifi_device_get_aps(NMDeviceWifi *wifi, GParamSpec *pspec,
                                QuickSettingsGridWifiMenu *self);

// stub out dispose, finalize, init and class init functions for GObject
static void quick_settings_grid_wifi_menu_dispose(GObject *object) {
    QuickSettingsGridWifiMenu *self = QUICK_SETTINGS_GRID_WIFI_MENU(object);

    GtkWidget *child =
        gtk_widget_get_first_child(GTK_WIDGET(self->menu.options));
    while (child) {
        QuickSettingsGridWifiMenuOption *opt =
            g_object_get_data(G_OBJECT(child), "opt");

        gtk_box_remove(self->menu.options, child);

        child = gtk_widget_get_first_child(GTK_WIDGET(self->menu.options));

        g_object_unref(opt);
    }

    // disconnect from signal
    g_signal_handlers_disconnect_by_func(self->dev, wifi_device_get_aps, self);
}

static void quick_settings_grid_wifi_menu_finalize(GObject *object) {}

static void quick_settings_grid_wifi_menu_class_init(
    QuickSettingsGridWifiMenuClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = quick_settings_grid_wifi_menu_dispose;
    object_class->finalize = quick_settings_grid_wifi_menu_finalize;

    // define signal
    g_signal_new("password-entry-revealed", G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
                 QUICK_SETTINGS_GRID_WIFI_MENU_OPTION_TYPE);
}

static void wifi_device_get_aps(NMDeviceWifi *wifi, GParamSpec *pspec,
                                QuickSettingsGridWifiMenu *self) {
    const GPtrArray *aps = nm_device_wifi_get_access_points(wifi);
    NMDeviceState state = nm_device_get_state(NM_DEVICE(wifi));
    NMAccessPoint *active = nm_device_wifi_get_active_access_point(wifi);

    g_debug(
        "quick_settings_grid_wifi_menu.c:wifi_device_get_aps() called, device "
        "state "
        "[%d]",
        state);

    if (state == NM_DEVICE_STATE_FAILED) {
        g_debug("quick_settings_grid_wifi_menu.c:wifi_device_get_aps() failed");
        gtk_revealer_set_reveal_child(self->menu.banner, true);
    }
    if (state == NM_DEVICE_STATE_ACTIVATED) {
        gtk_revealer_set_reveal_child(self->menu.banner, false);
    }

    g_debug("quick_settings_grid_wifi_menu.c:wifi_device_get_aps() called");

    // remove all options from self->menu.container
    GtkWidget *child =
        gtk_widget_get_first_child(GTK_WIDGET(self->menu.options));
    while (child) {
        QuickSettingsGridWifiMenuOption *opt =
            g_object_get_data(G_OBJECT(child), "opt");

        gtk_box_remove(self->menu.options, child);

        child = gtk_widget_get_first_child(GTK_WIDGET(self->menu.options));

        g_object_unref(opt);
    }

    // if we have a state which suggests the device should not be read for ap's
    // don't.
    // funny enough when you disable your wifi card with "nmcli radio wifi off "
    // it will send all the last seen APs for one event additional event.
    switch (state) {
        case NM_DEVICE_STATE_UNKNOWN:
        case NM_DEVICE_STATE_UNAVAILABLE:
        case NM_DEVICE_STATE_UNMANAGED:
            return;
        default:;
    }

    QuickSettingsGridWifiMenuOption *active_option = NULL;

    // check if we have a pointer to this ap in our hash table
    // if not, add it.
    for (int i = 0; i < aps->len; i++) {
        NMAccessPoint *ap = g_ptr_array_index(aps, i);

        QuickSettingsGridWifiMenuOption *opt =
            g_object_new(QUICK_SETTINGS_GRID_WIFI_MENU_OPTION_TYPE, NULL);

        // attach pointer to opt on main widget's data
        g_object_set_data(
            G_OBJECT(quick_settings_grid_wifi_menu_option_get_widget(opt)),
            "opt", opt);

        quick_settings_grid_wifi_menu_option_set_ap(opt, self, wifi, ap);

        // check if this is the active ap
        if (active == ap) {
            active_option = opt;
            continue;
        }

        gtk_box_append(self->menu.options,
                       quick_settings_grid_wifi_menu_option_get_widget(opt));
    }

    // always keep active option on top
    if (active_option) {
        gtk_box_prepend(
            self->menu.options,
            quick_settings_grid_wifi_menu_option_get_widget(active_option));
    }
}

static void on_failure_banner_clicked(GtkButton *button,
                                      QuickSettingsGridWifiMenu *self) {
    gtk_revealer_set_reveal_child(self->menu.banner, false);
    QuickSettingsMediator *m = quick_settings_get_global_mediator();
    quick_settings_mediator_req_shrink(m);
}

static void quick_settings_grid_wifi_menu_init_layout(
    QuickSettingsGridWifiMenu *self) {
    quick_settings_menu_widget_init(&self->menu, true);
    quick_settings_menu_widget_set_icon(
        &self->menu, "network-wireless-signal-excellent-symbolic");
    quick_settings_menu_widget_set_title(&self->menu, "Wi-Fi");
    gtk_widget_set_size_request(GTK_WIDGET(self->menu.container), -1, 420);


    // create failure banner
    self->failure_banner_container =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->failure_banner_container),
                             "failure-banner");
    self->failure_banner_label =
        GTK_LABEL(gtk_label_new("Failed to connect to network"));
    gtk_widget_set_hexpand(GTK_WIDGET(self->failure_banner_label), TRUE);
    self->failure_banner_dismiss_button = GTK_BUTTON(gtk_button_new_with_label(
        gtk_label_get_text(self->failure_banner_label)));
    gtk_button_set_child(self->failure_banner_dismiss_button,
                         GTK_WIDGET(self->failure_banner_label));
    gtk_box_append(self->failure_banner_container,
                   GTK_WIDGET(self->failure_banner_dismiss_button));
    gtk_revealer_set_child(self->menu.banner,
                           GTK_WIDGET(self->failure_banner_container));

    // wire in button click
    g_signal_connect(self->failure_banner_dismiss_button, "clicked",
                     G_CALLBACK(on_failure_banner_clicked), self);

    // add spinner to title_container
    self->spinner = GTK_SPINNER(gtk_spinner_new());
    gtk_box_append(self->menu.title_container, GTK_WIDGET(self->spinner));
    gtk_widget_set_halign(GTK_WIDGET(self->spinner), GTK_ALIGN_END);
}

static void quick_settings_grid_wifi_menu_init(
    QuickSettingsGridWifiMenu *self) {
    self->cancel_scan = g_cancellable_new();
    self->ap_options = g_ptr_array_new();
    quick_settings_grid_wifi_menu_init_layout(self);
}

void quick_settings_grid_wifi_menu_set_device(QuickSettingsGridWifiMenu *self,
                                              NMDeviceWifi *dev) {
    self->dev = dev;

    // wire into active access point change
    g_signal_connect(self->dev, "notify::state",
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
    }
    wifi_device_get_aps(dev, NULL, self);
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