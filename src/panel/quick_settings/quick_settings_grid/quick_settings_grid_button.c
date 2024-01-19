#include "quick_settings_grid_button.h"

#include <adwaita.h>

#include "quick_settings_grid_ethernet.h"
#include "./quick_settings_grid_wifi/quick_settings_grid_wifi.h"

void quick_settings_grid_button_init(QuickSettingsGridButton *self,
                                     enum QuickSettingsButtonType type,
                                     const gchar *title, const gchar *subtitle,
                                     const gchar *icon_name,
                                     gboolean with_reveal) {
    self->type = type;

    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_hexpand(GTK_WIDGET(self->container), TRUE);
    gtk_widget_set_size_request(GTK_WIDGET(self->container), 100, -1);
    gtk_widget_add_css_class(GTK_WIDGET(self->container),
                             "quick-settings-grid-button");

    // create and append toggle button
    self->toggle = GTK_BUTTON(gtk_button_new());
    gtk_widget_add_css_class(GTK_WIDGET(self->toggle),
                             "quick-settings-grid-button-toggle");
    gtk_widget_set_hexpand(GTK_WIDGET(self->toggle), TRUE);

    // append pointer to self on container's data
    g_object_set_data(G_OBJECT(self->toggle), "self", self);

    // GtkCenterBox *center_box = GTK_CENTER_BOX(gtk_center_box_new());
    GtkBox *button_contents =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    // create and append icon
    if (icon_name) {
        self->icon = GTK_IMAGE(gtk_image_new_from_icon_name(icon_name));
        gtk_widget_add_css_class(GTK_WIDGET(self->icon),
                                 "quick-settings-grid-button-icon");
        // left align
        gtk_widget_set_halign(GTK_WIDGET(self->icon), GTK_ALIGN_START);
        gtk_box_append(button_contents, GTK_WIDGET(self->icon));
    }
    GtkBox *text_area = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
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
    gtk_box_append(button_contents, GTK_WIDGET(text_area));
    gtk_button_set_child(self->toggle, GTK_WIDGET(button_contents));
    gtk_box_append(self->container, GTK_WIDGET(self->toggle));

    self->reveal =
        GTK_BUTTON(gtk_button_new_from_icon_name("go-next-symbolic"));
    gtk_widget_add_css_class(GTK_WIDGET(self->reveal),
                             "quick-settings-grid-button-reveal-hidden");
    gtk_box_append(self->container, GTK_WIDGET(self->reveal));

    if (with_reveal) {
        gtk_widget_add_css_class(GTK_WIDGET(self->reveal),
                                 "quick-settings-grid-button-reveal-visible");
    }
}

QuickSettingsGridButton *quick_settings_grid_button_new(
    enum QuickSettingsButtonType type, const gchar *title,
    const gchar *subtitle, const gchar *icon_name, gboolean with_reveal) {
    QuickSettingsGridButton *self = g_malloc0(sizeof(QuickSettingsGridButton));

    quick_settings_grid_button_init(self, type, title, subtitle, icon_name,
                                    with_reveal);

    return self;
}

void quick_settings_grid_button_free(QuickSettingsGridButton *self) {
    switch (self->type) {
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
    }
}