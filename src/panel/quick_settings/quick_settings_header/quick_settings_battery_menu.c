#include "quick_settings_battery_menu.h"

#include <adwaita.h>

#include "../../../services/upower_service.h"
#include "../quick_settings.h"
#include "../quick_settings_menu_widget.h"
#include "gtk/gtk.h"

enum signals { signals_n };

typedef struct _QuickSettingsBatteryMenu {
    QuickSettingsMenuWidget menu;
    GtkScale *battery_scale;
    GtkLabel *battery_time;
    GtkLabel *battery_percentage;
    GSettings *settings;
} QuickSettingsBatteryMenu;
G_DEFINE_TYPE(QuickSettingsBatteryMenu, quick_settings_battery_menu,
              G_TYPE_OBJECT);

static void on_power_dev_notify(UpDevice *power_dev, GParamSpec *pspec,
                                QuickSettingsBatteryMenu *self) {
    g_debug("quick_settings_battery_button.c:on_power_dev_notify() called.");

    gchar *icon_name = upower_device_map_icon_name(power_dev);
    gtk_image_set_from_icon_name(self->menu.icon, icon_name);

    gboolean is_recharable = false;
    g_object_get(power_dev, "is-rechargeable", &is_recharable, NULL);
    if (!is_recharable) {
        gtk_range_set_value(GTK_RANGE(self->battery_scale), 100);
        gtk_label_set_text(self->battery_time, "");
    }

    double percent = 0;
    guint state = 0;
    g_object_get(power_dev, "percentage", &percent, NULL);
    g_object_get(power_dev, "state", &state, NULL);

    // update battery scale
    gtk_range_set_value(GTK_RANGE(self->battery_scale), percent);

    // update percentage
    gchar *percentage_str = g_strdup_printf("%.0f%%", percent);
    gtk_label_set_text(self->battery_percentage, percentage_str);
    g_free(percentage_str);

    gboolean charging = (state == UP_DEVICE_STATE_CHARGING ||
                         state == UP_DEVICE_STATE_FULLY_CHARGED ||
                         state == UP_DEVICE_STATE_PENDING_CHARGE);

    gchar *time_str = NULL;
    if (charging) {
        gint64 time_to_full = 0;
        g_object_get(power_dev, "time-to-full", &time_to_full, NULL);

        gint64 hours = (time_to_full / (60 * 60));
        gint64 minutes = (time_to_full / 60) % 60;

        if (hours) {
            time_str = g_strdup_printf("%ld Hours %ld Minutes Until Full",
                                       hours, minutes);
        } else {
            time_str = g_strdup_printf("%ld Minutes Until Full", minutes);
        }
    } else {
        gint64 time_to_empty = 0;
        g_object_get(power_dev, "time-to-empty", &time_to_empty, NULL);

        gint64 hours = (time_to_empty / (60 * 60));
        gint64 minutes = (time_to_empty / 60) % 60;

        if (hours) {
            time_str = g_strdup_printf("%ld Hours %ld Minutes Remaining", hours,
                                       minutes);
        } else {
            time_str = g_strdup_printf("%ld Minutes Remaining", minutes);
        }
    }

    gtk_label_set_text(self->battery_time, time_str);
    g_debug("Battery time: %s", time_str);
    g_free(time_str);
}

static void on_quick_settings_hidden(QuickSettings *qs,
                                     QuickSettingsBatteryMenu *self) {
    g_debug("quick_settings_battery_menu.c:on_quick_settings_hidden() called.");
}

// stub out empty dispose, finalize, class_init, and init methods for this
// GObject.
static void quick_settings_battery_menu_dispose(GObject *gobject) {
    g_debug(
        "quick_settings_battery_menu.c:quick_settings_battery_menu_dispose() "
        "called.");

    // kill signals
    QuickSettings *qs = quick_settings_get_global();
    g_signal_handlers_disconnect_by_func(qs, on_quick_settings_hidden, gobject);

    UPowerService *upower = upower_service_get_global();
    UpDevice *power_dev = upower_service_get_primary_device(upower);
    g_signal_handlers_disconnect_by_func(power_dev, on_power_dev_notify,
                                         gobject);

    // Chain-up
    G_OBJECT_CLASS(quick_settings_battery_menu_parent_class)->dispose(gobject);
};

static void quick_settings_battery_menu_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(quick_settings_battery_menu_parent_class)->finalize(gobject);
};

static void quick_settings_battery_menu_class_init(
    QuickSettingsBatteryMenuClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = quick_settings_battery_menu_dispose;
    object_class->finalize = quick_settings_battery_menu_finalize;
};

static void quick_settings_battery_menu_init_layout(
    QuickSettingsBatteryMenu *self) {
    g_debug(
        "quick_settings_battery_menu.c:quick_settings_battery_menu_init() "
        "called.");

    quick_settings_menu_widget_init(&self->menu, false);
    quick_settings_menu_widget_set_title(&self->menu, "Battery");
    quick_settings_menu_widget_set_icon(&self->menu, "battery-full-symbolic");

    GtkBox *container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_name(GTK_WIDGET(container), "battery-menu");
    GtkBox *stats_container =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

    self->battery_scale = GTK_SCALE(
        gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1));
    // do not allow interacting with the scale
    gtk_widget_set_sensitive(GTK_WIDGET(self->battery_scale), FALSE);

    self->battery_time = GTK_LABEL(gtk_label_new(NULL));
    gtk_label_set_ellipsize(self->battery_time, PANGO_ELLIPSIZE_END);
    gtk_label_set_width_chars(self->battery_time, 30);
    gtk_label_set_xalign(self->battery_time, 0.0);

    self->battery_percentage = GTK_LABEL(gtk_label_new(NULL));

    gtk_box_append(container, GTK_WIDGET(self->battery_scale));
    gtk_box_append(stats_container, GTK_WIDGET(self->battery_time));
    gtk_box_append(stats_container, GTK_WIDGET(self->battery_percentage));
    gtk_box_append(container, GTK_WIDGET(stats_container));

    gtk_box_append(self->menu.options, GTK_WIDGET(container));

    UPowerService *upower = upower_service_get_global();

    // get primary power device
    UpDevice *power_dev = upower_service_get_primary_device(upower);

    on_power_dev_notify(power_dev, NULL, self);

    g_signal_connect(power_dev, "notify", G_CALLBACK(on_power_dev_notify),
                     self);

    // wire into quick settings hidden event and close all revealers
    QuickSettings *qs = quick_settings_get_global();
    g_signal_connect(qs, "quick-settings-hidden",
                     G_CALLBACK(on_quick_settings_hidden), self);
}

void quick_settings_battery_menu_reinitialize(QuickSettingsBatteryMenu *self) {
    // kill signals to quick settings mediator
    QuickSettings *qs = quick_settings_get_global();
    g_signal_handlers_disconnect_by_func(qs, on_quick_settings_hidden, self);

    // kill signals to power device
    UPowerService *upower = upower_service_get_global();
    UpDevice *power_dev = upower_service_get_primary_device(upower);
    g_signal_handlers_disconnect_by_func(power_dev, on_power_dev_notify, self);

    quick_settings_battery_menu_init_layout(self);
}

static void quick_settings_battery_menu_init(QuickSettingsBatteryMenu *self) {
    quick_settings_battery_menu_init_layout(self);
}

GtkWidget *quick_settings_battery_menu_get_widget(
    QuickSettingsBatteryMenu *self) {
    return GTK_WIDGET(self->menu.container);
}
