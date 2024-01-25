#include "panel_status_bar_sound_button.h"

#include <adwaita.h>

#include "../../services/wireplumber_service.h"
#include "wp/node.h"

struct _PanelStatusBarSoundButton {
    GObject parent_instance;
    GtkImage *speaker_icon;
    GtkImage *microphone_icon;
    GtkBox *container;
    guint32 signal_id;
    guint32 active_mic_signal_id;
    guint32 default_sink_signal_id;
};
G_DEFINE_TYPE(PanelStatusBarSoundButton, panel_status_bar_sound_button,
              G_TYPE_OBJECT);

static void on_default_nodes_changed(WirePlumberService *wp, guint32 id,
                                     PanelStatusBarSoundButton *self) {
    WirePlumberServiceDefaultNodes nodes;
    wire_plumber_service_get_default_nodes(wp, &nodes);

    g_debug(
        "panel_status_bar_sound_button.c:on_default_nodes_changed() called.");

    return;

    if (id == nodes.sink.id || id == 0) {
        if (nodes.sink.mute) {
            gtk_image_set_from_icon_name(self->speaker_icon,
                                         "audio-volume-muted-symbolic");
            goto source;
        }
        if (nodes.sink.volume < 0.25) {
            gtk_image_set_from_icon_name(self->speaker_icon,
                                         "audio-volume-low-symbolic");
        }
        if (nodes.sink.volume >= 0.25 && nodes.sink.volume < 0.5) {
            gtk_image_set_from_icon_name(self->speaker_icon,
                                         "audio-volume-medium-symbolic");
        }
        if (nodes.sink.volume >= 0.50) {
            gtk_image_set_from_icon_name(self->speaker_icon,
                                         "audio-volume-high-symbolic");
        }
    }
source:
    if (id == nodes.source.id || id == 0) {
        // debug active state
        g_debug(
            "panel_status_bar_sound_button.c:on_default_nodes_changed() source "
            "active: %d",
            nodes.source.active);
        if (nodes.source.active) {
            // set icon visible if source is active
            gtk_widget_set_visible(GTK_WIDGET(self->microphone_icon), true);
        } else {
            // set icon invisible if source is inactive
            gtk_widget_set_visible(GTK_WIDGET(self->microphone_icon), false);
            return;
        }

        if (nodes.source.mute) {
            gtk_image_set_from_icon_name(
                self->microphone_icon, "microphone-sensitivity-muted-symbolic");
            return;
        }
        if (nodes.source.volume < 0.25) {
            gtk_image_set_from_icon_name(self->microphone_icon,
                                         "microphone-sensitivity-low-symbolic");
        }
        if (nodes.source.volume >= 0.25 && nodes.source.volume < 0.5) {
            gtk_image_set_from_icon_name(
                self->microphone_icon,
                "microphone-sensitivity-medium-symbolic");
        }
        if (nodes.source.volume >= 0.50) {
            gtk_image_set_from_icon_name(
                self->microphone_icon, "microphone-sensitivity-high-symbolic");
        }
    }

    return;
}

static void on_microphone_active(WirePlumberService *wp, gboolean active,
                                 PanelStatusBarSoundButton *self) {
    g_debug(
        "panel_status_bar_sound_button.c:on_microphone_active() called. "
        "active: "
        "%d",
        active);
    if (active) {
        // set icon visible if source is active
        gtk_widget_set_visible(GTK_WIDGET(self->microphone_icon), true);
    } else {
        // set icon invisible if source is inactive
        gtk_widget_set_visible(GTK_WIDGET(self->microphone_icon), false);
        return;
    }
}

static void on_default_sink_changed(WirePlumberService *wp,
                                    WirePlumberServiceNode *sink,
                                    PanelStatusBarSoundButton *self) {
    if (!sink) {
        return;
    }
    g_debug(
        "panel_status_bar_sound_button.c:on_default_sink_changed() called. id: "
        "%d volume: %f name: %s",
        sink->id, sink->volume, sink->name);

    if (sink->mute) {
        gtk_image_set_from_icon_name(self->speaker_icon,
                                     "audio-volume-muted-symbolic");
        return;
    }
    if (sink->volume < 0.25) {
        gtk_image_set_from_icon_name(self->speaker_icon,
                                     "audio-volume-low-symbolic");
    }
    if (sink->volume >= 0.25 && sink->volume < 0.5) {
        gtk_image_set_from_icon_name(self->speaker_icon,
                                     "audio-volume-medium-symbolic");
    }
    if (sink->volume >= 0.50) {
        gtk_image_set_from_icon_name(self->speaker_icon,
                                     "audio-volume-high-symbolic");
    }
}

// stub out dispose, finalize, class_init, and init methods
static void panel_status_bar_sound_button_dispose(GObject *gobject) {
    PanelStatusBarSoundButton *self = PANEL_STATUS_BAR_SOUND_BUTTON(gobject);

    // disconnect from signals
    WirePlumberService *wps = wire_plumber_service_get_global();

    g_signal_handler_disconnect(wps, self->active_mic_signal_id);
    g_signal_handler_disconnect(wps, self->default_sink_signal_id);

    // Chain-up
    G_OBJECT_CLASS(panel_status_bar_sound_button_parent_class)
        ->dispose(gobject);
};

static void panel_status_bar_sound_button_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(panel_status_bar_sound_button_parent_class)
        ->finalize(gobject);
};

static void panel_status_bar_sound_button_class_init(
    PanelStatusBarSoundButtonClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = panel_status_bar_sound_button_dispose;
    object_class->finalize = panel_status_bar_sound_button_finalize;
};

static void panel_status_bar_sound_button_init_layout(
    PanelStatusBarSoundButton *self) {
    WirePlumberService *wps = wire_plumber_service_get_global();

    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

    // create icon
    self->speaker_icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("audio-volume-muted-symbolic"));
    self->microphone_icon = GTK_IMAGE(
        gtk_image_new_from_icon_name("microphone-sensitivity-high-symbolic"));

    // attach both icons to container
    gtk_box_append(self->container, GTK_WIDGET(self->microphone_icon));
    gtk_box_append(self->container, GTK_WIDGET(self->speaker_icon));

    on_default_nodes_changed(wps, 0, self);

    on_microphone_active(wps, wire_plumber_service_microphone_active(wps),
                         self);
    on_default_sink_changed(wps, wire_plumber_service_get_default_sink(wps),
                            self);

    // self->signal_id =
    //     g_signal_connect(wps, "default_nodes_changed",
    //                      G_CALLBACK(on_default_nodes_changed), self);
    self->active_mic_signal_id = g_signal_connect(
        wps, "microphone-active", G_CALLBACK(on_microphone_active), self);
    self->default_sink_signal_id = g_signal_connect(
        wps, "default-sink-changed", G_CALLBACK(on_default_sink_changed), self);
};

static void panel_status_bar_sound_button_init(
    PanelStatusBarSoundButton *self) {
    panel_status_bar_sound_button_init_layout(self);
}

GtkWidget *panel_status_bar_sound_button_get_widget(
    PanelStatusBarSoundButton *self) {
    return GTK_WIDGET(self->container);
}