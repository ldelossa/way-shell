#include "quick_settings_menu_widget.h"

void quick_settings_menu_widget_init(QuickSettingsMenuWidget *self,
                                     gboolean scrolling) {
    // create container for entire widget
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->container), "quick-settings-menu");

    // create container for options area
    self->options_container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->options_container), "container");

    // create title box [icon label]
    self->title_container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->title_container), "title-container");

    // create icon area in title box
    self->title_icon_container =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->title_icon_container),
                        "icon-container");
    self->icon = GTK_IMAGE(gtk_image_new_from_icon_name(""));
    gtk_image_set_pixel_size(self->icon, 24);

    // append icon to title box and add label adjacent.
    gtk_box_append(self->title_icon_container, GTK_WIDGET(self->icon));
    gtk_box_append(self->title_container,
                   GTK_WIDGET(self->title_icon_container));
    self->title = GTK_LABEL(gtk_label_new(""));
    gtk_box_append(self->title_container, GTK_WIDGET(self->title));

    // create banner revealer
    self->banner = GTK_REVEALER(gtk_revealer_new());
    gtk_widget_set_name(GTK_WIDGET(self->banner), "banner");
    gtk_revealer_set_transition_type(self->banner,
                                     GTK_REVEALER_TRANSITION_TYPE_SWING_DOWN);
    gtk_revealer_set_transition_duration(self->banner, 400);

    // create button container
    self->options = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->options), "options-container");

    // wire it up
    gtk_box_append(self->container, GTK_WIDGET(self->options_container));
    gtk_box_append(self->options_container, GTK_WIDGET(self->title_container));
    gtk_box_append(self->options_container, GTK_WIDGET(self->banner));
    if (scrolling) {
        self->scroll = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
        gtk_widget_set_size_request(GTK_WIDGET(self->scroll), -1, 240);
        gtk_scrolled_window_set_child(self->scroll, GTK_WIDGET(self->options));
        gtk_box_append(self->options_container, GTK_WIDGET(self->scroll));
    } else
        gtk_box_append(self->options_container, GTK_WIDGET(self->options));
}

void quick_settings_menu_widget_set_title(QuickSettingsMenuWidget *self,
                                          const char *title) {
    gtk_label_set_text(GTK_LABEL(self->title), title);
}

void quick_settings_menu_widget_set_icon(QuickSettingsMenuWidget *self,
                                         const char *icon_name) {
    gtk_image_set_from_icon_name(self->icon, icon_name);
}