#include "quick_settings_grid_night_light_menu.h"

#include <adwaita.h>

#include "../../../../services/wayland_service/wayland_service.h"
#include "../../quick_settings.h"
#include "../../quick_settings_menu_widget.h"
#include "gtk/gtk.h"
#include "quick_settings_grid_night_light.h"

enum signals { signals_n };

typedef struct _QuickSettingsGridNightLightMenu {
    GObject parent_instance;
    QuickSettingsMenuWidget menu;
    GtkScale *temp_slider;
} QuickSettingsGridNightLightMenu;
G_DEFINE_TYPE(QuickSettingsGridNightLightMenu,
              quick_settings_grid_night_light_menu, G_TYPE_OBJECT);

// stub out dispose, finalize, init and class init functions for GObject
static void quick_settings_grid_night_light_menu_dispose(GObject *object) {
    QuickSettingsGridNightLightMenu *self =
        QUICK_SETTINGS_GRID_NIGHT_LIGHT_MENU(object);
}

static void quick_settings_grid_night_light_menu_finalize(GObject *object) {}

static void quick_settings_grid_night_light_menu_class_init(
    QuickSettingsGridNightLightMenuClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = quick_settings_grid_night_light_menu_dispose;
    object_class->finalize = quick_settings_grid_night_light_menu_finalize;
}

static void on_value_changed(GtkRange *range,
                             QuickSettingsGridNightLightMenu *self) {
    g_debug("quick_settings_grid_night_light_menu.c:on_value_changed() called");

    WaylandService *w = wayland_service_get_global();
    wayland_wlr_bluelight_filter(w, gtk_range_get_value(range));
}

static void quick_settings_grid_night_light_menu_init_layout(
    QuickSettingsGridNightLightMenu *self) {
    quick_settings_menu_widget_init(&self->menu, false);
    gtk_image_set_from_icon_name(self->menu.icon, "night-light-symbolic");
    gtk_label_set_text(self->menu.title, "Temperature");

    // create temperature scale
    self->temp_slider = GTK_SCALE(
        gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1000, 6800, 500));

    for (int i = 1000; i <= 6000; i += 1000) {
        char *str = g_strdup_printf("%dK", i / 1000);
        gtk_scale_add_mark(self->temp_slider, i, GTK_POS_BOTTOM, str);
        g_free(str);
    }

	gtk_range_set_value(GTK_RANGE(self->temp_slider), 3000);

    // connect signal
    g_signal_connect(self->temp_slider, "value-changed",
                     G_CALLBACK(on_value_changed), self);

    gtk_box_append(self->menu.options, GTK_WIDGET(self->temp_slider));
}

static void quick_settings_grid_night_light_menu_init(
    QuickSettingsGridNightLightMenu *self) {
    quick_settings_grid_night_light_menu_init_layout(self);
}

void quick_settings_grid_night_light_menu_on_reveal(
    QuickSettingsGridButton *button_, gboolean is_revealed) {
    QuickSettingsGridNightLightButton *button =
        (QuickSettingsGridNightLightButton *)button_;
    QuickSettingsGridNightLightMenu *self = button->menu;
    g_debug("quick_settings_grid_night_light_menu.c:on_reveal() called");
}

GtkWidget *quick_settings_grid_night_light_menu_get_widget(
    QuickSettingsGridNightLightMenu *self) {
    return GTK_WIDGET(self->menu.container);
}

GtkScale *quick_settings_grid_night_light_menu_get_temp_scale(
    QuickSettingsGridNightLightMenu *grid) {
    return grid->temp_slider;
}
