#include "quick_settings_audio_scales.h"

#include <adwaita.h>

#include "../../services/wireplumber_service.h"

static void on_default_nodes_changed(WirePlumberService *self, guint32 id,
                                     AudioScales *scales) {
    WirePlumberServiceDefaultNodes nodes;
    wire_plumber_service_get_default_nodes(self, &nodes);

    g_debug("quick_settings_audio_scales.c:on_default_nodes_changed() called.");

    if (id == nodes.sink.id || id == 0) {
        if (nodes.sink.mute) {
            gtk_image_set_from_icon_name(scales->default_sink.icon,
                                         "audio-volume-muted-symbolic");
            gtk_range_set_value(GTK_RANGE(scales->default_sink.scale), 0);
            goto source;
        }
        if (nodes.sink.volume < 0.25) {
            gtk_image_set_from_icon_name(scales->default_sink.icon,
                                         "audio-volume-low-symbolic");
        }
        if (nodes.sink.volume >= 0.25 && nodes.sink.volume < 0.5) {
            gtk_image_set_from_icon_name(scales->default_sink.icon,
                                         "audio-volume-medium-symbolic");
        }
        if (nodes.sink.volume >= 0.50) {
            gtk_image_set_from_icon_name(scales->default_sink.icon,
                                         "audio-volume-high-symbolic");
        }
        gtk_range_set_value(GTK_RANGE(scales->default_sink.scale),
                            nodes.sink.volume);
    }
source:
    if (id == nodes.source.id || id == 0) {
        if (nodes.source.active) {
            // make default source container visible
            gtk_widget_set_visible(GTK_WIDGET(scales->default_source.container),
                                   true);
        } else {
            // make default source container invisible
            gtk_widget_set_visible(GTK_WIDGET(scales->default_source.container),
                                   false);
            return;
        }

        if (nodes.source.mute) {
            gtk_image_set_from_icon_name(
                scales->default_source.icon,
                "microphone-sensitivity-muted-symbolic");
            gtk_range_set_value(GTK_RANGE(scales->default_source.scale), 0);
            return;
        }
        if (nodes.source.volume < 0.25) {
            gtk_image_set_from_icon_name(scales->default_source.icon,
                                         "microphone-sensitivity-low-symbolic");
        }
        if (nodes.source.volume >= 0.25 && nodes.source.volume < 0.5) {
            gtk_image_set_from_icon_name(
                scales->default_source.icon,
                "microphone-sensitivity-medium-symbolic");
        }
        if (nodes.source.volume >= 0.50) {
            gtk_image_set_from_icon_name(
                scales->default_source.icon,
                "microphone-sensitivity-high-symbolic");
        }
        gtk_range_set_value(GTK_RANGE(scales->default_source.scale),
                            nodes.source.volume);
    }
};

void quick_settings_audio_scales_init(AudioScales *scales) {
    WirePlumberService *wp = wire_plumber_service_get_global();

    scales->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_name(GTK_WIDGET(scales->container), "audio-scales");

    // init default sink
    scales->default_sink.container =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

    gtk_widget_set_name(GTK_WIDGET(scales->default_sink.container),
                        "default-sink-container");

    scales->default_sink.icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("audio-volume-high-symbolic"));

    scales->default_sink.scale = GTK_SCALE(
        gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 1.0, .5));
    gtk_widget_set_hexpand(GTK_WIDGET(scales->default_sink.scale), true);

    // append icon as first child to scale container
    gtk_box_append(scales->default_sink.container,
                   GTK_WIDGET(scales->default_sink.icon));

    // append scale to container
    gtk_box_append(scales->default_sink.container,
                   GTK_WIDGET(scales->default_sink.scale));

    // init default source
    scales->default_source.container =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_name(GTK_WIDGET(scales->default_source.container),
                        "default-source-container");

    scales->default_source.icon = GTK_IMAGE(
        gtk_image_new_from_icon_name("microphone-sensitivity-high-symbolic"));

    scales->default_source.scale = GTK_SCALE(
        gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 1.0, .5));
    gtk_widget_set_hexpand(GTK_WIDGET(scales->default_source.scale), true);

    // append icon as first child to scale container
    gtk_box_append(scales->default_source.container,
                   GTK_WIDGET(scales->default_source.icon));

    // append scale to container
    gtk_box_append(scales->default_source.container,
                   GTK_WIDGET(scales->default_source.scale));

    // append default node containers to main container
    gtk_box_append(scales->container,
                   GTK_WIDGET(scales->default_sink.container));
    gtk_box_append(scales->container,
                   GTK_WIDGET(scales->default_source.container));

    // wire to default-nodes-changed event
    g_signal_connect(wp, "default-nodes-changed",
                     G_CALLBACK(on_default_nodes_changed), scales);

    // request an event from wp
    wire_plumber_service_default_nodes_req(wp);
}

void quick_settings_audio_scales_free(AudioScales *scales) {
    g_object_unref(scales->container);
    g_object_unref(scales->default_sink.container);
    g_object_unref(scales->default_sink.icon);
    g_object_unref(scales->default_sink.scale);
    g_object_unref(scales->default_source.container);
    g_object_unref(scales->default_source.icon);
    g_object_unref(scales->default_source.scale);
}
