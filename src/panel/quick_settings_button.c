#include "quick_settings_button.h"

#include <adwaita.h>
#include <upower.h>

#include "../services/upower_service.h"
#include "../services/wireplumber_service.h"
#include "panel.h"
#include "panel_mediator.h"

struct _QuickSettingsButton {
    GObject parent_instance;
    GtkBox *container;
    GtkButton *button;
    GtkBox *box;
    GtkImage *icons[icons_n];
    Panel *panel;
    gboolean toggled;
};
G_DEFINE_TYPE(QuickSettingsButton, quick_settings_button, G_TYPE_OBJECT);

static void quick_settings_button_dispose(GObject *gobject) {
    QuickSettingsButton *self = QUICK_SETTINGS_BUTTON(gobject);
    G_OBJECT_CLASS(quick_settings_button_parent_class)->dispose(gobject);
};

static void quick_settings_button_finalize(GObject *gobject) {
    G_OBJECT_CLASS(quick_settings_button_parent_class)->finalize(gobject);
};

static void quick_settings_button_class_init(QuickSettingsButtonClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = quick_settings_button_dispose;
    object_class->finalize = quick_settings_button_finalize;
};

GtkImage *quick_settings_button_get_icon(QuickSettingsButton *self,
                                         enum IconTypes pos) {
    GtkWidget *icon = gtk_widget_get_first_child(GTK_WIDGET(self->box));

    if (pos > icons_n) {
        return NULL;
    }

    for (int i = 0; i != pos; i++) {
        icon = gtk_widget_get_next_sibling(icon);
    }
    return GTK_IMAGE(icon);
};

static void on_click(GtkButton *button, QuickSettingsButton *self) {
    g_debug("quick_settings_button.c:on_click() called.");
    PanelMediator *mediator = panel_get_global_mediator();
    panel_mediator_emit_qs_toggle_request(mediator, self->panel);
};

static void on_battery_icon_change(GObject *object, GParamSpec *pspec,
                                   QuickSettingsButton *qs) {
    char *icon_name = NULL;

    g_debug("quick_settings_button.c:on_battery_icon_change() called.");

    icon_name = upower_device_map_icon_name(UP_DEVICE(object));

    g_debug("quick_settings_button.c:on_battery_icon_change() icon_name: %s",
            icon_name);

    qs_button_set_icon(qs, power, icon_name);
}

static void on_default_nodes_changed(WirePlumberService *self, guint32 id,
                                     QuickSettingsButton *qs) {
    WirePlumberServiceDefaultNodes nodes;
    wire_plumber_service_get_default_nodes(self, &nodes);

    g_debug("quick_settings_button.c:on_default_nodes_changed() called.");

    if (id == nodes.sink.id || id == 0) {
        if (nodes.sink.mute) {
            qs_button_set_icon(qs, sound, "audio-volume-muted-symbolic");
            goto source;
        }
        if (nodes.sink.volume < 0.25) {
            qs_button_set_icon(qs, sound, "audio-volume-low-symbolic");
        }
        if (nodes.sink.volume >= 0.25 && nodes.sink.volume < 0.5) {
            qs_button_set_icon(qs, sound, "audio-volume-medium-symbolic");
        }
        if (nodes.sink.volume >= 0.50) {
            qs_button_set_icon(qs, sound, "audio-volume-high-symbolic");
        }
    }
source:
    if (id == nodes.source.id || id == 0) {
        // debug active state
        g_debug(
            "quick_settings_button.c:on_default_nodes_changed() source "
            "active: %d",
            nodes.source.active);
        if (nodes.source.active) {
            // set icon visible if source is active
            gtk_widget_set_visible(
                GTK_WIDGET(quick_settings_button_get_icon(qs, microphone)),
                true);
        } else {
            // set icon invisible if source is inactive
            gtk_widget_set_visible(
                GTK_WIDGET(quick_settings_button_get_icon(qs, microphone)),
                false);
            return;
        }

        if (nodes.source.mute) {
            qs_button_set_icon(qs, microphone,
                               "microphone-sensitivity-muted-symbolic");
            return;
        }
        if (nodes.source.volume < 0.25) {
            qs_button_set_icon(qs, microphone,
                               "microphone-sensitivity-low-symbolic");
        }
        if (nodes.source.volume >= 0.25 && nodes.source.volume < 0.5) {
            qs_button_set_icon(qs, microphone,
                               "microphone-sensitivity-medium-symbolic");
        }
        if (nodes.source.volume >= 0.50) {
            qs_button_set_icon(qs, microphone,
                               "microphone-sensitivity-high-symbolic");
        }
    }

    return;
}

static void quick_settings_button_init_upower(QuickSettingsButton *self) {
    gboolean is_bat = FALSE;
    UpDevice *bat = NULL;
    char *icon_name = NULL;
    UPowerService *upower = upower_service_get_global();
    WirePlumberService *wps = wire_plumber_service_get_global();

    if (!upower) {
        return;
    }

    if (!wps) {
        return;
    }

    is_bat = upower_service_primary_is_bat(upower);
    // if we don't have a battery just set our power icon to always use the AC
    // symbolic icon
    if (!is_bat) {
        qs_button_set_icon(self, power, "ac-adapter-symbolic");
        return;
    }

    // we have a battery so get the UPowerDevice and hook up a signal to it
    // which handles battery change events
    bat = upower_service_get_primary_device(upower);
    if (!bat) {
        return;
    }

    // set initial icon in button based on retrieved bat
    icon_name = upower_device_map_icon_name(bat);
    qs_button_set_icon(self, power, icon_name);

    // connect to "notify::icon-name" on UpDevice to update icon on battery
    // status changes.
    g_signal_connect(bat, "notify::icon-name",
                     G_CALLBACK(on_battery_icon_change), self);

    // connect to wireplumber default nodes changes
    g_signal_connect(wps, "default_nodes_changed",
                     G_CALLBACK(on_default_nodes_changed), self);
}

static void quick_settings_button_init(QuickSettingsButton *self) {
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    self->button = GTK_BUTTON(gtk_button_new());
    g_signal_connect(self->button, "clicked", G_CALLBACK(on_click), self);

    self->box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_orientable_set_orientation(GTK_ORIENTABLE(self->box),
                                   GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_add_css_class(GTK_WIDGET(self->button), "panel-button");

    // append in left-to-right order following enum IconTypes.

    // these start invisible
    gtk_box_append(self->box, gtk_image_new_from_icon_name(
                                  "bluetooth-disconnected-symbolic"));
    gtk_widget_set_visible(
        GTK_WIDGET(quick_settings_button_get_icon(self, bluetooth)), false);

    gtk_box_append(self->box, gtk_image_new_from_icon_name(
                                  "microphone-sensitivity-high-symbolic"));
    gtk_widget_set_visible(
        GTK_WIDGET(quick_settings_button_get_icon(self, microphone)), false);

    // these start visible.
    gtk_box_append(self->box, gtk_image_new_from_icon_name(
                                  "network-wireless-offline-symbolic"));
    gtk_box_append(self->box, gtk_image_new_from_icon_name(
                                  "audio-volume-medium-symbolic"));
    gtk_box_append(self->box,
                   gtk_image_new_from_icon_name("battery-full-symbolic"));
    gtk_button_set_child(self->button, GTK_WIDGET(self->box));
    gtk_box_append(self->container, GTK_WIDGET(self->button));
    quick_settings_button_init_upower(self);
};

GtkButton *quick_settings_button_get_button(QuickSettingsButton *self) {
    return GTK_BUTTON(gtk_widget_get_first_child(GTK_WIDGET(self->container)));
};

GtkBox *quick_settings_button_get_widget(QuickSettingsButton *self) {
    return self->container;
};

int qs_button_set_icon(QuickSettingsButton *b, enum IconTypes pos,
                       const char *name) {
    GtkImage *icon = NULL;

    if (pos > icons_n) {
        return -1;
    }

    icon = quick_settings_button_get_icon(b, pos);
    gtk_image_set_from_icon_name(icon, name);
    return 0;
};

void quick_settings_button_set_panel(QuickSettingsButton *self, Panel *panel) {
    g_object_ref(panel);
    self->panel = panel;
};

void quick_settings_button_set_toggled(QuickSettingsButton *self,
                                       gboolean toggled) {
    if (toggled) {
        gtk_widget_add_css_class(GTK_WIDGET(self->button),
                                 "panel-button-toggled");
    } else {
        gtk_widget_remove_css_class(GTK_WIDGET(self->button),
                                    "panel-button-toggled");
    }
    self->toggled = toggled;
}

gboolean quick_settings_get_toggled(QuickSettingsButton *self) {
    return self->toggled;
}