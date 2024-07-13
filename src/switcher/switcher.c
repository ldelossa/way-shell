#include "switcher.h"

#include <adwaita.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

#include "gtk/gtk.h"

void switcher_init(Switcher *self, gboolean has_list) {
    // create key controller
    self->key_controller = gtk_event_controller_key_new();

    // configure Switcher's window
    self->win = ADW_WINDOW(adw_window_new());
    gtk_widget_set_hexpand(GTK_WIDGET(self->win), true);
    gtk_widget_set_vexpand(GTK_WIDGET(self->win), true);
    gtk_widget_set_name(GTK_WIDGET(self->win), "switcher");
    gtk_widget_set_visible(GTK_WIDGET(self->win), false);
    gtk_widget_add_controller(GTK_WIDGET(self->win), self->key_controller);
    gtk_window_set_default_size(GTK_WINDOW(self->win), 600, 0);
    // configure layershell, no anchors will place window in center.
    gtk_layer_init_for_window(GTK_WINDOW(self->win));
    gtk_layer_set_namespace(GTK_WINDOW(self->win), "way-shell-switcher");
    gtk_layer_set_layer((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_anchor((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_EDGE_TOP,
                         true);
    gtk_layer_set_margin((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_EDGE_TOP,
                         400);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(self->win),
                                GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);

    // create main container
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_hexpand(GTK_WIDGET(self->container), true);
    gtk_widget_set_vexpand(GTK_WIDGET(self->container), true);

    // create search box
    GtkBox *search_container =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_name(GTK_WIDGET(search_container), "search-container");
    gtk_widget_set_hexpand(GTK_WIDGET(search_container), true);
    self->search_entry = GTK_SEARCH_ENTRY(gtk_search_entry_new());
    gtk_widget_set_name(GTK_WIDGET(self->search_entry), "search-entry");
    gtk_box_append(search_container, GTK_WIDGET(self->search_entry));

    // create scrolled win for list
    self->scrolled = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
    gtk_scrolled_window_set_max_content_height(self->scrolled, 400);
    gtk_scrolled_window_set_propagate_natural_height(self->scrolled, 1);
    gtk_scrolled_window_set_propagate_natural_width(self->scrolled, 1);

    // create list
    self->list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_widget_set_name(GTK_WIDGET(self->list), "list");
    gtk_widget_set_hexpand(GTK_WIDGET(self->list), true);
    gtk_widget_set_vexpand(GTK_WIDGET(self->list), true);

    gtk_scrolled_window_set_child(self->scrolled, GTK_WIDGET(self->list));

    // wire it up
    gtk_box_append(self->container, GTK_WIDGET(search_container));
    if (has_list)
        gtk_box_append(self->container, GTK_WIDGET(self->scrolled));
    else
        gtk_widget_add_css_class(GTK_WIDGET(self->container), "no_list");

    adw_window_set_content(self->win, GTK_WIDGET(self->container));
}

GtkListBoxRow *switcher_top_choice(Switcher *self) {
    // always select first visible entry
    GtkListBoxRow *child =
        GTK_LIST_BOX_ROW(gtk_widget_get_first_child(GTK_WIDGET(self->list)));
    while (child) {
        // I found that you must use gtk_widget_get_child_visible by trial and
        // error...
        if (gtk_widget_get_child_visible(GTK_WIDGET(child))) {
            return child;
            break;
        }
        child =
            GTK_LIST_BOX_ROW(gtk_widget_get_next_sibling(GTK_WIDGET(child)));
    }
    return NULL;
}

GtkListBoxRow *switcher_last_choice(Switcher *self) {
    // always select first visible entry
    GtkListBoxRow *found = NULL;
    GtkListBoxRow *child =
        GTK_LIST_BOX_ROW(gtk_widget_get_first_child(GTK_WIDGET(self->list)));
    while (child) {
        if (gtk_widget_get_child_visible(GTK_WIDGET(child))) {
            found = child;
        }
        child =
            GTK_LIST_BOX_ROW(gtk_widget_get_next_sibling(GTK_WIDGET(child)));
    }
    return found;
}
