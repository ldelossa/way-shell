#include "quick_settings_grid_button.h"

#include <adwaita.h>

#include "../quick_settings.h"
#include "./quick_settings_grid_airplane_mode_button.h"
#include "./quick_settings_grid_idle_inhibitor.h"
#include "./quick_settings_grid_night_light/quick_settings_grid_night_light.h"
#include "./quick_settings_grid_power_profiles/quick_settings_grid_power_profiles.h"
#include "./quick_settings_grid_theme.h"
#include "./quick_settings_grid_vpn/quick_settings_grid_vpn.h"
#include "./quick_settings_grid_wifi/quick_settings_grid_wifi.h"
#include "./quick_settings_keyboard_brightness/quick_settings_grid_keyboard_brightness.h"
#include "gtk/gtk.h"
#include "gtk/gtkrevealer.h"
#include "quick_settings_grid_ethernet.h"

static void quick_settings_grid_button_on_reveal(
    GtkButton *button, QuickSettingsGridButton *self) {
    g_debug(
        "quick_settings_grid_button.c:quick_settings_grid_button_on_reveal() "
        "called");

    QuickSettings *qs = quick_settings_get_global();

    if (gtk_revealer_get_child_revealed(self->revealer)) {
        gtk_revealer_set_reveal_child(self->revealer, false);

        quick_settings_shrink(qs);
        quick_settings_set_focused(qs, false);

        if (self->on_reveal) self->on_reveal(self, false);
    } else {
        // emit will-reveal signal on behalf of our cluster
        g_signal_emit_by_name(self->cluster, "will-reveal", self);

        quick_settings_set_focused(qs, true);

        gtk_revealer_set_reveal_child(self->revealer, true);
        if (self->on_reveal) self->on_reveal(self, true);
    }
}

void quick_settings_grid_button_init(
    QuickSettingsGridButton *self, enum QuickSettingsButtonType type,
    const gchar *title, const gchar *subtitle, const gchar *icon_name,
    GtkWidget *reveal_widget, QuickSettingsClusterOnRevealFunc on_reveal) {
    self->type = type;
    self->reveal_widget = reveal_widget;
    self->on_reveal = on_reveal;

    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_hexpand(GTK_WIDGET(self->container), TRUE);
    gtk_widget_set_size_request(GTK_WIDGET(self->container), 100, 60);
    gtk_widget_add_css_class(GTK_WIDGET(self->container),
                             "quick-settings-grid-button");

    // create and append toggle button
    self->toggle = GTK_BUTTON(gtk_button_new());
    gtk_widget_add_css_class(GTK_WIDGET(self->toggle),
                             "quick-settings-grid-button-toggle");
    gtk_widget_set_hexpand(GTK_WIDGET(self->toggle), TRUE);

    // append pointer to self on container's data
    g_object_set_data(G_OBJECT(self->toggle), "self", self);

    GtkOverlay *overlay = GTK_OVERLAY(gtk_overlay_new());
    gtk_overlay_set_child(overlay, GTK_WIDGET(self->toggle));

    // GtkCenterBox *center_box = GTK_CENTER_BOX(gtk_center_box_new());
    GtkBox *button_contents =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

    // valign center
    gtk_widget_set_valign(GTK_WIDGET(button_contents), GTK_ALIGN_CENTER);
    // create and append icon
    if (icon_name) {
        self->icon = GTK_IMAGE(gtk_image_new_from_icon_name(icon_name));
        gtk_image_set_pixel_size(self->icon, 20);
        gtk_widget_add_css_class(GTK_WIDGET(self->icon),
                                 "quick-settings-grid-button-icon");
        // left align
        gtk_widget_set_halign(GTK_WIDGET(self->icon), GTK_ALIGN_START);
        gtk_box_append(button_contents, GTK_WIDGET(self->icon));
    }
    GtkBox *text_area = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->toggle), "with-subtitle");
    if (title) {
        self->title = GTK_LABEL(gtk_label_new(title));
        gtk_label_set_ellipsize(self->title, PANGO_ELLIPSIZE_END);
        gtk_label_set_width_chars(self->title, 12);
        gtk_label_set_max_width_chars(self->title, 12);
        gtk_label_set_xalign(self->title, 0.0);
        gtk_widget_add_css_class(GTK_WIDGET(self->title),
                                 "quick-settings-grid-button-title");
        // left align
        gtk_widget_set_halign(GTK_WIDGET(self->title), GTK_ALIGN_START);
        gtk_box_append(text_area, GTK_WIDGET(self->title));
    }
    if (subtitle) {
        self->subtitle = GTK_LABEL(gtk_label_new(subtitle));
        gtk_label_set_ellipsize(self->subtitle, PANGO_ELLIPSIZE_END);
        gtk_label_set_width_chars(self->subtitle, 12);
        gtk_label_set_max_width_chars(self->subtitle, 12);
        gtk_label_set_xalign(self->subtitle, 0.0);
        gtk_widget_add_css_class(GTK_WIDGET(self->subtitle),
                                 "quick-settings-grid-button-subtitle");
        // left align
        gtk_widget_set_halign(GTK_WIDGET(self->subtitle), GTK_ALIGN_START);
        gtk_box_append(text_area, GTK_WIDGET(self->subtitle));
    }
    // if no subtitle adjust title so its centered
    if (!subtitle) {
        gtk_widget_set_vexpand(GTK_WIDGET(self->title), TRUE);
        gtk_widget_set_valign(GTK_WIDGET(self->title), GTK_ALIGN_CENTER);
        gtk_widget_remove_css_class(GTK_WIDGET(self->toggle), "with-subtitle");
    }
    gtk_box_append(button_contents, GTK_WIDGET(text_area));
    gtk_button_set_child(self->toggle, GTK_WIDGET(button_contents));
    gtk_box_append(self->container, GTK_WIDGET(overlay));

    // setup revealer
    self->revealer = GTK_REVEALER(gtk_revealer_new());
    gtk_revealer_set_transition_type(self->revealer,
                                     GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(self->revealer, 350);
    gtk_revealer_set_reveal_child(self->revealer, false);
    gtk_widget_add_css_class(GTK_WIDGET(self->revealer),
                             "quick-settings-grid-button-revealer");

    if (self->reveal_widget) {
        gtk_widget_add_css_class(GTK_WIDGET(self->toggle), "with-revealer");
        self->reveal_button =
            GTK_BUTTON(gtk_button_new_from_icon_name("go-next-symbolic"));
        gtk_widget_add_css_class(GTK_WIDGET(self->reveal_button),
                                 "quick-settings-grid-button-reveal-hidden");

        gtk_widget_set_halign(GTK_WIDGET(self->reveal_button), GTK_ALIGN_END);

        gtk_overlay_add_overlay(overlay, GTK_WIDGET(self->reveal_button));

        // add reveal_widget as child of revealer
        gtk_revealer_set_child(self->revealer, self->reveal_widget);

        // wire reveal button to revealer
        g_signal_connect(self->reveal_button, "clicked",
                         G_CALLBACK(quick_settings_grid_button_on_reveal),
                         self);

        gtk_widget_add_css_class(GTK_WIDGET(self->reveal_button),
                                 "quick-settings-grid-button-reveal-visible");
    }
}

QuickSettingsGridButton *quick_settings_grid_button_new(
    enum QuickSettingsButtonType type, const gchar *title,
    const gchar *subtitle, const gchar *icon_name, GtkWidget *reveal_widget,
    QuickSettingsClusterOnRevealFunc on_reveal) {
    QuickSettingsGridButton *self = g_malloc0(sizeof(QuickSettingsGridButton));

    quick_settings_grid_button_init(self, type, title, subtitle, icon_name,
                                    reveal_widget, on_reveal);

    return self;
}

void quick_settings_grid_button_set_toggled(QuickSettingsGridButton *self,
                                            gboolean toggled) {
    if (toggled) {
        gtk_widget_remove_css_class(GTK_WIDGET(self->toggle), "off");
        gtk_widget_remove_css_class(GTK_WIDGET(self->reveal_button), "off");
    } else {
        gtk_widget_add_css_class(GTK_WIDGET(self->toggle), "off");
        gtk_widget_add_css_class(GTK_WIDGET(self->reveal_button), "off");
    }
}

void quick_settings_grid_button_free(QuickSettingsGridButton *self) {
    switch (self->type) {
        case QUICK_SETTINGS_BUTTON_ONESHOT:
        case QUICK_SETTINGS_BUTTON_GENERIC:
            break;
        case QUICK_SETTINGS_BUTTON_ETHERNET:
            quick_settings_grid_ethernet_button_free(
                (QuickSettingsGridEthernetButton *)self);
            break;
        case QUICK_SETTINGS_BUTTON_WIFI:
            quick_settings_grid_wifi_button_free(
                (QuickSettingsGridWifiButton *)self);
            break;
        case QUICK_SETTINGS_BUTTON_PERFORMANCE:
            quick_settings_grid_power_profiles_button_free(
                (QuickSettingsGridPowerProfilesButton *)self);
            break;
        case QUICK_SETTINGS_BUTTON_IDLE_INHIBITOR:
            quick_settings_grid_inhibitor_button_free(
                (QuickSettingsGridIdleInhibitorButton *)self);
            break;
        case QUICK_SETTINGS_BUTTON_THEME:
            quick_settings_grid_theme_button_free(
                (QuickSettingsGridOneThemeButton *)self);
        case QUICK_SETTINGS_BUTTON_NIGHT_LIGHT:
            quick_settings_grid_night_light_button_free(
                (QuickSettingsGridNightLightButton *)self);
            break;
        case QUICK_SETTINGS_BUTTON_AIRPLANE_MODE:
            quick_settings_grid_airplane_mode_button_free(
                (QuickSettingsGridAirplaneModeButton *)self);
            break;
        case QUICK_SETTINGS_BUTTON_KEYBOARD_BRIGHTNESS:
            quick_settings_grid_keyboard_brightness_button_free(
                (QuickSettingsGridKeyboardBrightnessButton *)self);
            break;
        case QUICK_SETTINGS_BUTTON_VPN:
            quick_settings_grid_vpn_button_free(
                (QuickSettingsGridVPNButton *)self);
            break;
    }
}
