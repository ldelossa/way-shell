#include "quick_settings_scales.h"

#include <adwaita.h>

#include "../../../services/brightness_service/brightness_service.h"
#include "../../../services/wireplumber_service.h"
#include "gtk/gtkrevealer.h"

enum signals { signals_n };

typedef struct _QuickSettingsScales {
    GObject parent_instance;
    GtkBox *container;
    GtkBox *audio_scales_revealer_contents;
    GtkRevealer *audio_scales_revealer;
    GtkBox *default_sink_container;
    GtkImage *default_sink_icon;
    GtkButton *default_sink_button;
    GtkScale *default_sink_scale;
    GtkBox *default_source_container;
    GtkImage *default_source_icon;
    GtkButton *default_source_button;
    GtkScale *default_source_scale;
    GtkBox *brightness_container;
    GtkImage *brightness_icon;
    GtkButton *brightness_button;
    GtkScale *brightness_scale;
    // wirepumber nodes
    WirePlumberServiceNode *active_source_node;
    WirePlumberServiceNode *default_sink_node;
} QuickSettingsScales;
G_DEFINE_TYPE(QuickSettingsScales, quick_settings_scales, G_TYPE_OBJECT);

// foward declare callbacks
static void on_default_source_change(WirePlumberService *wp,
                                     WirePlumberServiceNode *source,
                                     QuickSettingsScales *self);
static void on_default_sink_change(WirePlumberService *wp,
                                   WirePlumberServiceNode *sink,
                                   QuickSettingsScales *self);

// stub out empty dispose, finalize, class_init, and init methods for this
// GObject.
static void quick_settings_scales_dispose(GObject *gobject) {
    g_debug("quick_settings_scales.c:quick_settings_scales_dispose() called.");

    // disconnect from signals
    WirePlumberService *wp = wire_plumber_service_get_global();

    QuickSettingsScales *self = QUICK_SETTINGS_SCALES(gobject);

    g_signal_handlers_disconnect_by_func(
        wp, G_CALLBACK(on_default_source_change), self);

    g_signal_handlers_disconnect_by_func(wp, G_CALLBACK(on_default_sink_change),
                                         self);

    g_signal_handlers_disconnect_by_func(wp, G_CALLBACK(on_default_sink_change),
                                         self);

    // Chain-up
    G_OBJECT_CLASS(quick_settings_scales_parent_class)->dispose(gobject);
};

static void quick_settings_scales_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(quick_settings_scales_parent_class)->finalize(gobject);
};

static void quick_settings_scales_class_init(QuickSettingsScalesClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose = quick_settings_scales_dispose;
    gobject_class->finalize = quick_settings_scales_finalize;

    g_debug(
        "quick_settings_scales.c:quick_settings_scales_class_init() called.");
};

static void on_default_source_change(WirePlumberService *wp,
                                     WirePlumberServiceNode *source,
                                     QuickSettingsScales *self);

static void on_source_scale_value_changed(GtkRange *range,
                                          QuickSettingsScales *self);

static void block_default_source_changed_signals(QuickSettingsScales *self,
                                                 gboolean block) {
    WirePlumberService *wp = wire_plumber_service_get_global();
    if (block)
        g_signal_handlers_block_by_func(
            wp, G_CALLBACK(on_default_source_change), self);
    else
        g_signal_handlers_unblock_by_func(
            wp, G_CALLBACK(on_default_source_change), self);
}

static void block_default_source_scale_changed_signals(
    QuickSettingsScales *self, gboolean block) {
    if (block)
        g_signal_handlers_block_by_func(
            GTK_RANGE(self->default_source_scale),
            G_CALLBACK(on_source_scale_value_changed), self);
    else
        g_signal_handlers_unblock_by_func(
            GTK_RANGE(self->default_source_scale),
            G_CALLBACK(on_source_scale_value_changed), self);
}

static void on_source_scale_value_changed(GtkRange *range,
                                          QuickSettingsScales *self) {
    g_debug("quick_settings_scales.c:on_sink_scale_value_changed() called.");

    gdouble r = gtk_range_get_value(range);
    WirePlumberService *wp = wire_plumber_service_get_global();
    WirePlumberServiceNode *default_source =
        wire_plumber_service_get_default_source(wp);
    if (!default_source) return;

    // we are going to write to the mixer-api, so ignore the next wireplumber
    // event
    block_default_source_changed_signals(self, true);

    wire_plumber_service_set_volume(wp, default_source, r);
    // manually map icon since we turn off wire plumber events in here
    gchar *icon = wire_plumber_service_map_source_vol_icon(r, false);
    gtk_image_set_from_icon_name(self->default_source_icon, icon);

    // unblock our blocked signal
    block_default_source_changed_signals(self, false);
}

static void on_default_source_change(WirePlumberService *wp,
                                     WirePlumberServiceNode *source,
                                     QuickSettingsScales *self) {
    g_debug("quick_settings_scales.c:on_default_source_change() called.");

    if (!source) return;

    // we are going to update our sliders here, so block the event
    // which would occur when the slider is changed
    block_default_source_scale_changed_signals(self, true);

    if (source->state == WP_NODE_STATE_RUNNING) {
        gtk_widget_set_visible(GTK_WIDGET(self->default_source_container),
                               true);
    } else {
        gtk_widget_set_visible(GTK_WIDGET(self->default_source_container),
                               false);
    }

    gchar *icon =
        wire_plumber_service_map_source_vol_icon(source->volume, source->mute);

    self->active_source_node = source;

    gtk_image_set_from_icon_name(self->default_source_icon, icon);

    if (source->mute)
        gtk_range_set_value(GTK_RANGE(self->default_source_scale), 0);
    else
        gtk_range_set_value(GTK_RANGE(self->default_source_scale),
                            self->active_source_node->volume);

    // unblock our blocked signal
    block_default_source_scale_changed_signals(self, false);
}

static void on_default_sink_change(WirePlumberService *wp,
                                   WirePlumberServiceNode *sink,
                                   QuickSettingsScales *self);

static void on_sink_scale_value_changed(GtkRange *range,
                                        QuickSettingsScales *self);

static void block_default_sink_changed_signals(QuickSettingsScales *self,
                                               gboolean block) {
    WirePlumberService *wp = wire_plumber_service_get_global();
    if (block)
        g_signal_handlers_block_by_func(wp, G_CALLBACK(on_default_sink_change),
                                        self);
    else
        g_signal_handlers_unblock_by_func(
            wp, G_CALLBACK(on_default_sink_change), self);
}

static void block_default_sink_scale_changed_signals(QuickSettingsScales *self,
                                                     gboolean block) {
    if (block)
        g_signal_handlers_block_by_func(GTK_RANGE(self->default_sink_scale),
                                        G_CALLBACK(on_sink_scale_value_changed),
                                        self);
    else
        g_signal_handlers_unblock_by_func(
            GTK_RANGE(self->default_sink_scale),
            G_CALLBACK(on_sink_scale_value_changed), self);
}

static void on_sink_scale_value_changed(GtkRange *range,
                                        QuickSettingsScales *self) {
    g_debug("quick_settings_scales.c:on_sink_scale_value_changed() called.");

    gdouble r = gtk_range_get_value(range);
    WirePlumberService *wp = wire_plumber_service_get_global();
    WirePlumberServiceNode *default_sink =
        wire_plumber_service_get_default_sink(wp);
    if (!default_sink) return;

    // we are going to write to the mixer-api, so ignore the next wireplumber
    // event
    block_default_sink_changed_signals(self, true);

    wire_plumber_service_set_volume(wp, default_sink, r);
    // manually map icon since we turn off wire plumber events in here
    gchar *icon = wire_plumber_service_map_sink_vol_icon(r, false);
    gtk_image_set_from_icon_name(self->default_sink_icon, icon);

    // unblock our blocked signal
    block_default_sink_changed_signals(self, false);
}

static void on_default_sink_change(WirePlumberService *wp,
                                   WirePlumberServiceNode *sink,
                                   QuickSettingsScales *self) {
    g_debug("quick_settings_scales.c:on_default_sink_change() called.");

    if (!sink) return;

    // we are going to update our sliders here, so block the event
    // which would occur when the slider is changed
    block_default_sink_scale_changed_signals(self, true);

    // set scale to sink's volume
    gtk_range_set_value(GTK_RANGE(self->default_sink_scale), sink->volume);

    gchar *icon =
        wire_plumber_service_map_sink_vol_icon(sink->volume, sink->mute);

    self->default_sink_node = sink;

    gtk_image_set_from_icon_name(self->default_sink_icon, icon);

    if (sink->mute)
        gtk_range_set_value(GTK_RANGE(self->default_sink_scale), 0);
    else
        gtk_range_set_value(GTK_RANGE(self->default_sink_scale),
                            self->default_sink_node->volume);

    // unblock our blocked signal
    block_default_sink_scale_changed_signals(self, false);
}

static void on_brightness_scale_changed(GtkRange *range,
                                        QuickSettingsScales *self);

static void on_brightness_change(BrightnessService *bs, float percent,
                                 QuickSettingsScales *self);

static void block_brightness_scale_changed_signals(QuickSettingsScales *self,
                                                   gboolean block) {
    if (block)
        g_signal_handlers_block_by_func(GTK_RANGE(self->brightness_scale),
                                        G_CALLBACK(on_brightness_scale_changed),
                                        self);
    else
        g_signal_handlers_unblock_by_func(
            GTK_RANGE(self->brightness_scale),
            G_CALLBACK(on_brightness_scale_changed), self);
}

static void block_brightness_changed_signals(QuickSettingsScales *self,
                                             gboolean block) {
    BrightnessService *bs = brightness_service_get_global();
    if (block)
        g_signal_handlers_block_by_func(bs, G_CALLBACK(on_brightness_change),
                                        self);
    else
        g_signal_handlers_unblock_by_func(bs, G_CALLBACK(on_brightness_change),
                                          self);
}

static void on_brightness_scale_changed(GtkRange *range,
                                        QuickSettingsScales *self) {
    g_debug("quick_settings_scales.c:on_brightness_scale_changed() called.");

    // we are going to set brightness so ignore the next brightness changed
    // event.

    block_brightness_changed_signals(self, true);

    gdouble r = gtk_range_get_value(range);
    BrightnessService *bs = brightness_service_get_global();
    brightness_service_set_backlight(bs, r);

    block_brightness_changed_signals(self, false);
}

static void on_brightness_change(BrightnessService *bs, float percent,
                                 QuickSettingsScales *self) {
    g_debug("quick_settings_scales.c:on_brightness_changed() called.");

    // we are going to change our scale setting here, so ignore the event
    // fired from this.
    block_brightness_scale_changed_signals(self, true);

    // set scale with new brightness
    gtk_range_set_value(GTK_RANGE(self->brightness_scale), percent);

    block_brightness_scale_changed_signals(self, false);
}

static void on_brightness_enter(GtkEventControllerMotion *ctlr, double x,
                                double y, QuickSettingsScales *self) {
    g_debug("quick_settings_scales.c:on_brightness_enter() called.");

    // pause default-node-changed events
    BrightnessService *bs = brightness_service_get_global();

    g_signal_handlers_block_by_func(bs, G_CALLBACK(on_brightness_change), self);

    // unblock gtk scale handler
    g_signal_handlers_unblock_by_func(GTK_RANGE(self->brightness_scale),
                                      G_CALLBACK(on_brightness_change), self);
}

static void on_brightness_leave(GtkEventControllerMotion *ctlr, double x,
                                double y, QuickSettingsScales *self) {
    g_debug("quick_settings_scales.c:on_brightness_leave() called.");
    // unpause default-node-changed events
    BrightnessService *bs = brightness_service_get_global();

    // block gtk scale handler
    g_signal_handlers_block_by_func(GTK_RANGE(self->brightness_scale),
                                    G_CALLBACK(on_brightness_change), self);

    g_signal_handlers_unblock_by_func(bs, G_CALLBACK(on_brightness_change),
                                      self);
}

static void on_default_sink_button_clicked(GtkButton *button,
                                           QuickSettingsScales *self) {
    g_debug("quick_settings_scales.c:on_default_sink_button_clicked() called.");

    WirePlumberService *wp = wire_plumber_service_get_global();
    if (!self->default_sink_node) return;
    if (!self->default_sink_node->mute)
        wire_plumber_service_volume_mute(wp, self->default_sink_node);
    else
        wire_plumber_service_volume_unmute(wp, self->default_sink_node);
}

static void on_active_source_button_clicked(GtkButton *button,
                                            QuickSettingsScales *self) {
    g_debug(
        "quick_settings_scales.c:on_active_source_button_clicked() called.");

    WirePlumberService *wp = wire_plumber_service_get_global();
    if (!self->active_source_node) return;
    if (!self->active_source_node->mute)
        wire_plumber_service_volume_mute(wp, self->active_source_node);
    else
        wire_plumber_service_volume_unmute(wp, self->active_source_node);
}

static void quick_settings_scales_init_layout(QuickSettingsScales *self) {
    // create container
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->container), "quick-settings-scales");

    // create audio scales revealer
    self->audio_scales_revealer = GTK_REVEALER(gtk_revealer_new());
    gtk_revealer_set_transition_type(self->audio_scales_revealer,
                                     GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
    gtk_revealer_set_transition_duration(self->audio_scales_revealer, 250);
    gtk_revealer_set_reveal_child(self->audio_scales_revealer, TRUE);

    self->audio_scales_revealer_contents =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_revealer_set_child(self->audio_scales_revealer,
                           GTK_WIDGET(self->audio_scales_revealer_contents));

    // default sinks setup
    self->default_sink_container =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->default_sink_container),
                        "default-sink-container");

    self->default_sink_button = GTK_BUTTON(
        gtk_button_new_from_icon_name("audio-volume-muted-symbolic"));

    g_signal_connect(self->default_sink_button, "clicked",
                     G_CALLBACK(on_default_sink_button_clicked), self);

    self->default_sink_icon = GTK_IMAGE(
        gtk_widget_get_first_child(GTK_WIDGET(self->default_sink_button)));

    self->default_sink_scale = GTK_SCALE(
        gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 1, 0.05));
    gtk_widget_set_hexpand(GTK_WIDGET(self->default_sink_scale), true);

    gtk_box_append(self->default_sink_container,
                   GTK_WIDGET(self->default_sink_button));
    gtk_box_append(self->default_sink_container,
                   GTK_WIDGET(self->default_sink_scale));

    // default source setup
    self->default_source_container =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->default_source_scale),
                        "default-source-container");

    // start source as hidden
    gtk_widget_set_visible(GTK_WIDGET(self->default_source_container), false);

    self->default_source_scale = GTK_SCALE(
        gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 1, 0.05));
    gtk_widget_set_hexpand(GTK_WIDGET(self->default_source_scale), true);

    self->default_source_button = GTK_BUTTON(
        gtk_button_new_from_icon_name("microphone-muted-high-symbolic"));

    g_signal_connect(self->default_source_button, "clicked",
                     G_CALLBACK(on_active_source_button_clicked), self);

    self->default_source_icon = GTK_IMAGE(
        gtk_widget_get_first_child(GTK_WIDGET(self->default_source_button)));

    gtk_box_append(self->default_source_container,
                   GTK_WIDGET(self->default_source_button));
    gtk_box_append(self->default_source_container,
                   GTK_WIDGET(self->default_source_scale));

    WirePlumberService *wp = wire_plumber_service_get_global();
    WirePlumberServiceNode *default_source =
        wire_plumber_service_get_default_sink(wp);
    on_default_sink_change(wp, default_source, self);

    WirePlumberServiceNode *default_sink =
        wire_plumber_service_get_default_sink(wp);
    on_default_sink_change(wp, default_sink, self);

    g_signal_connect(wp, "default-source-changed",
                     G_CALLBACK(on_default_source_change), self);

    g_signal_connect(wp, "default-sink-changed",
                     G_CALLBACK(on_default_sink_change), self);

    // wire up to container
    gtk_box_append(self->audio_scales_revealer_contents,
                   GTK_WIDGET(self->default_sink_container));
    gtk_box_append(self->audio_scales_revealer_contents,
                   GTK_WIDGET(self->default_source_container));

    gtk_box_append(self->container, GTK_WIDGET(self->audio_scales_revealer));

    // brightness setup, dependent on whether brightness service is available.
    BrightnessService *bs = brightness_service_get_global();

    if (brightness_service_has_backlight_brightness(bs)) {
        self->brightness_container =
            GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
        gtk_widget_set_name(GTK_WIDGET(self->brightness_container),
                            "brightness-container");

        self->brightness_scale = GTK_SCALE(
            gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 1, 0.05));
        gtk_widget_set_hexpand(GTK_WIDGET(self->brightness_scale), true);

        self->brightness_button = GTK_BUTTON(
            gtk_button_new_from_icon_name(brightness_service_map_icon(bs)));

        self->brightness_icon = GTK_IMAGE(
            gtk_widget_get_first_child(GTK_WIDGET(self->brightness_button)));

        gtk_box_append(self->brightness_container,
                       GTK_WIDGET(self->brightness_button));
        gtk_box_append(self->brightness_container,
                       GTK_WIDGET(self->brightness_scale));

        // listen for brightness changes
        g_signal_connect(bs, "brightness-changed",
                         G_CALLBACK(on_brightness_change), self);

        // get initial brightness value
        float brightness = brightness_service_get_backlight(bs);
        gtk_range_set_value(GTK_RANGE(self->brightness_scale), brightness);

        // connect to scale's Range::value-changed signal
        g_signal_connect(GTK_RANGE(self->default_sink_scale), "value-changed",
                         G_CALLBACK(on_sink_scale_value_changed), self);

        g_signal_connect(GTK_RANGE(self->default_source_scale), "value-changed",
                         G_CALLBACK(on_source_scale_value_changed), self);

        g_signal_connect(GTK_RANGE(self->brightness_scale), "value-changed",
                         G_CALLBACK(on_brightness_scale_changed), self);

        gtk_box_append(self->container, GTK_WIDGET(self->brightness_container));
    }
}

static void quick_settings_scales_init(QuickSettingsScales *self) {
    g_debug("quick_settings_scales.c:quick_settings_scales_init() called.");
    quick_settings_scales_init_layout(self);
}

GtkWidget *quick_settings_scales_get_widget(QuickSettingsScales *self) {
    g_debug(
        "quick_settings_scales.c:quick_settings_scales_get_widget() called.");

    return GTK_WIDGET(self->container);
}

void quick_settings_scales_disable_audio_scales(QuickSettingsScales *self,
                                                gboolean disable) {
    WirePlumberService *wp = wire_plumber_service_get_global();
    if (!wp) return;

    // close revealer
    gtk_revealer_set_reveal_child(self->audio_scales_revealer, !disable);

    if (disable) {
        // block signals
        g_signal_handlers_block_by_func(
            wp, G_CALLBACK(on_default_source_change), self);
        g_signal_handlers_block_by_func(wp, G_CALLBACK(on_default_sink_change),
                                        self);
    } else {
        // unblock signals
        g_signal_handlers_unblock_by_func(
            wp, G_CALLBACK(on_default_source_change), self);
        g_signal_handlers_unblock_by_func(
            wp, G_CALLBACK(on_default_sink_change), self);

        // perform our events manually, since we may not have a signal coming.
        on_default_sink_change(wp, wire_plumber_service_get_default_sink(wp),
                               self);
        on_default_source_change(
            wp, wire_plumber_service_get_default_source(wp), self);
    }
}
