#include "quick_settings_power_menu.h"

#include <adwaita.h>

#include "../quick_settings.h"
#include "../quick_settings_mediator.h"
#include "../quick_settings_menu_widget.h"

enum signals { signals_n };

typedef struct _QuickSettingsPowerMenu {
    QuickSettingsMenuWidget menu;
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

    quick_settings_menu_widget_init(&self->menu, false);
    quick_settings_menu_widget_set_title(&self->menu, "Power Off");
    quick_settings_menu_widget_set_icon(&self->menu,
                                        "system-shutdown-symbolic");

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
    gtk_box_append(self->menu.options, GTK_WIDGET(self->suspend));
    gtk_box_append(self->menu.options, GTK_WIDGET(self->restart));
    gtk_box_append(self->menu.options, GTK_WIDGET(self->power_off));

    // append separator
    GtkSeparator *separator = GTK_SEPARATOR(gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_widget_set_margin_start(GTK_WIDGET(separator), 12);
    gtk_box_append(self->menu.options, GTK_WIDGET(self->logout));

}

void quick_settings_power_menu_reinitialize(QuickSettingsPowerMenu *self) {
    quick_settings_power_menu_init_layout(self);
}

static void quick_settings_power_menu_init(QuickSettingsPowerMenu *self) {
    quick_settings_power_menu_init_layout(self);
}

GtkWidget *quick_settings_power_menu_get_widget(QuickSettingsPowerMenu *self) {
    return GTK_WIDGET(self->menu.container);
}