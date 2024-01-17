#include "quick_settings_power_menu.h"

#include <adwaita.h>

enum signals { signals_n };

typedef struct _QuickSettingsPowerMenu {
    GtkBox *container;
    GtkBox *options_container;
    GtkBox *options_title;
    GtkBox *buttons_container;
    GtkButton *suspend;
    GtkButton *restart;
    GtkButton *power_off;
    GtkButton *logout;
} QuickSettingsPowerMenu;
G_DEFINE_TYPE(QuickSettingsPowerMenu, quick_settings_power_menu, G_TYPE_OBJECT);

// stub out empty dispose, finalize, class_init, and init methods for this
// GObject.
static void quick_settings_power_menu_dispose(GObject *gobject) {
    g_debug(
        "quick_settings_power_menu.c:quick_settings_power_menu_dispose() "
        "called.");

    // Chain-up
    G_OBJECT_CLASS(quick_settings_power_menu_parent_class)->dispose(gobject);
};

static void quick_settings_power_menu_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(quick_settings_power_menu_parent_class)->finalize(gobject);
};

static void quick_settings_power_menu_class_init(
    QuickSettingsPowerMenuClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = quick_settings_power_menu_dispose;
    object_class->finalize = quick_settings_power_menu_finalize;
};

static void quick_settings_power_menu_init_layout(
    QuickSettingsPowerMenu *self) {
    g_debug(
        "quick_settings_power_menu.c:quick_settings_power_menu_init() "
        "called.");

    // create container for entire widget
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->container), "power-menu");

    // create container for options area
    self->options_container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->options_container),
                        "options-container");

    // create title box [icon label]
    self->options_title = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->options_title), "options-title");

    // create power icon in title box
    GtkBox *icon_container =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_name(GTK_WIDGET(icon_container), "icon-container");
    GtkImage *icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("system-shutdown-symbolic"));
    gtk_image_set_pixel_size(icon, 24);

    // append icon to title box and add "Power Off" label adjacent.
    gtk_box_append(icon_container, GTK_WIDGET(icon));
    gtk_box_append(self->options_title, GTK_WIDGET(icon_container));
    gtk_box_append(self->options_title, gtk_label_new("Power Off"));

    // create button container
    self->buttons_container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->buttons_container),
                        "buttons-container");

    // create power buttons
    self->suspend = GTK_BUTTON(gtk_button_new_with_label("Suspend"));
    gtk_label_set_xalign(GTK_LABEL(gtk_button_get_child(self->suspend)), 0.0);
    self->restart = GTK_BUTTON(gtk_button_new_with_label("Restart..."));
    gtk_label_set_xalign(GTK_LABEL(gtk_button_get_child(self->restart)), 0.0);
    self->power_off = GTK_BUTTON(gtk_button_new_with_label("Power Off..."));
    gtk_label_set_xalign(GTK_LABEL(gtk_button_get_child(self->power_off)), 0.0);
    self->logout = GTK_BUTTON(gtk_button_new_with_label("Log Out..."));
    gtk_label_set_xalign(GTK_LABEL(gtk_button_get_child(self->logout)), 0.0);

    // wire it up

    // options_container child of container
    gtk_box_append(self->container, GTK_WIDGET(self->options_container));
    // title is first child of options_container
    gtk_box_append(self->options_container, GTK_WIDGET(self->options_title));
    // add all buttons to button container
    gtk_box_append(self->buttons_container, GTK_WIDGET(self->suspend));
    gtk_box_append(self->buttons_container, GTK_WIDGET(self->restart));
    gtk_box_append(self->buttons_container, GTK_WIDGET(self->power_off));
    gtk_box_append(self->buttons_container,
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(self->buttons_container, GTK_WIDGET(self->logout));

    // add buttons container as next child in options container
    gtk_box_append(self->options_container,
                   GTK_WIDGET(self->buttons_container));
}

void quick_settings_power_menu_reinitialize(
    QuickSettingsPowerMenu *self) {
    quick_settings_power_menu_init_layout(self);
}

static void quick_settings_power_menu_init(QuickSettingsPowerMenu *self) {
    quick_settings_power_menu_init_layout(self);
}

GtkWidget *quick_settings_power_menu_get_widget(QuickSettingsPowerMenu *self) {
    return GTK_WIDGET(self->container);
}