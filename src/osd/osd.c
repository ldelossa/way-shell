#include "./osd.h"

#include <adwaita.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

#include "../panel/quick_settings/quick_settings.h"
#include "../services/brightness_service/brightness_service.h"
#include "../services/wireplumber_service.h"
#include "gtk/gtkrevealer.h"

static OSD *global = NULL;

enum signals { signals_n };

typedef struct _OSD {
    GObject parent_instance;
    AdwWindow *win;
    GtkBox *container;
    GtkOverlay *overlay;
    GtkRevealer *volume_revealer;
    GtkScale *volume_scale;
    GtkImage *volume_icon;
    GtkRevealer *brightness_revealer;
    GtkScale *brightness_scale;
    GtkImage *bightness_icon;
    gboolean quick_settings_visible;
    guint32 timeout_id;
} OSD;
static guint osd_signals[signals_n] = {0};
G_DEFINE_TYPE(OSD, osd, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init and init methods.
static void osd_dispose(GObject *gobject) {
    OSD *self = OSD_OSD(gobject);

    // Chain-up
    G_OBJECT_CLASS(osd_parent_class)->dispose(gobject);
};

static void osd_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(osd_parent_class)->finalize(gobject);
};

static void osd_class_init(OSDClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = osd_dispose;
    object_class->finalize = osd_finalize;
};

static void do_cleanup(OSD *self) {
    g_debug("osd.c:do_cleanup(): called");

    if (self->timeout_id) {
        g_source_remove(self->timeout_id);
        self->timeout_id = 0;
    }
}

static void on_child_revealed(GObject *object, GParamSpec *pspec, OSD *self) {
    g_debug("osd.c:on_child_revealed(): called");

    gboolean volume_revealed =
        gtk_revealer_get_child_revealed(self->volume_revealer);
    gboolean brightness_revealed =
        gtk_revealer_get_child_revealed(self->brightness_revealer);
    g_debug(
        "osd.c:on_child_revealed(): volume_revealed: %d brightness_revealed: "
        "%d",
        volume_revealed, brightness_revealed);

    if (!volume_revealed && !brightness_revealed) {
        gtk_widget_set_visible(GTK_WIDGET(self->win), false);
    }
}

static gboolean timed_dismiss(OSD *self) {
    g_debug("osd.c:timed_dismiss(): called");

    if (!self->win) return false;
    // dismiss any open revealers
    gtk_revealer_set_reveal_child(self->brightness_revealer, false);
    gtk_revealer_set_reveal_child(self->volume_revealer, false);
    self->timeout_id = 0;
    return false;
}

static void on_brightness_changed(BrightnessService *bs, float percent,
                                  OSD *self) {
    g_debug("osd.c:on_brightness_changed(): called");

    // if quick settings is currently displayed, don't show brightness OSD, its
    // redunant
    QuickSettingsMediator *qs = quick_settings_get_global_mediator();
    if (quick_settings_mediator_qs_is_visible(qs)) return;

    // if a timeout exists cancel it
    if (self->timeout_id) {
        g_source_remove(self->timeout_id);
        self->timeout_id = 0;
    }

    char *icon_name = brightness_service_map_icon(bs);

    // set icon
    gtk_image_set_from_icon_name(self->bightness_icon, icon_name);

    // set brightness scale
    gtk_range_set_value(GTK_RANGE(self->brightness_scale), percent);

    // present window
    gtk_widget_set_visible(GTK_WIDGET(self->win), true);

    // reveal brightness osd
    gtk_revealer_set_reveal_child(self->brightness_revealer, true);

    // unreveal volume osd if necessary
    gtk_revealer_set_reveal_child(self->volume_revealer, false);

    self->timeout_id =
        g_timeout_add_seconds(8, (GSourceFunc)timed_dismiss, self);
}

static void on_default_sink_changed(WirePlumberService *wp,
                                    WirePlumberServiceNode *sink, OSD *self) {
    g_debug("osd.c:on_default_sink_changed()");

    // if quick settings is currently displayed, don't show volume OSD, its
    // redunant
    QuickSettingsMediator *qs = quick_settings_get_global_mediator();
    if (quick_settings_mediator_qs_is_visible(qs)) return;

    // if a timeout exists cancel it
    if (self->timeout_id) {
        g_source_remove(self->timeout_id);
        self->timeout_id = 0;
    }

    char *icon_name =
        wire_plumber_service_map_sink_vol_icon(sink->volume, sink->mute);

    // set icon
    gtk_image_set_from_icon_name(self->volume_icon, icon_name);

    // set audio scale
    gtk_range_set_value(GTK_RANGE(self->volume_scale), sink->volume);

    // present window
    gtk_widget_set_visible(GTK_WIDGET(self->win), true);

    // reveal audio osd
    gtk_revealer_set_reveal_child(self->volume_revealer, true);

    // unreveal brightness osd if necessary
    gtk_revealer_set_reveal_child(self->brightness_revealer, false);

    self->timeout_id =
        g_timeout_add_seconds(8, (GSourceFunc)timed_dismiss, self);
}

static void on_window_destroy(GtkWindow *win, OSD *self) {
    g_debug("osd.c:on_window_destroy() called");
    do_cleanup(self);
    osd_reinitialize(self);
}

static void on_quick_settings_will_show(QuickSettingsMediator *m,
                                        QuickSettings *qs, GdkMonitor *mon,
                                        OSD *self) {
    g_debug("osd.c:on_quick_settings_will_show() called");
    do_cleanup(self);
    gtk_revealer_set_reveal_child(self->volume_revealer, false);
    gtk_revealer_set_reveal_child(self->brightness_revealer, false);
}

void osd_init_layout(OSD *self) {
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));

    self->overlay = GTK_OVERLAY(gtk_overlay_new());
    gtk_overlay_set_child(self->overlay, GTK_WIDGET(self->container));

    self->win = ADW_WINDOW(adw_window_new());
    gtk_widget_set_size_request(GTK_WIDGET(self->win), 340, 64);

    // wire into window destroy event
    g_signal_connect(self->win, "destroy", G_CALLBACK(on_window_destroy), self);

    // configure layershell, top layer and center
    gtk_layer_init_for_window(GTK_WINDOW(self->win));
    gtk_layer_set_layer((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_widget_set_name(GTK_WIDGET(self->win), "osd");
    gtk_layer_set_anchor(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_BOTTOM,
                         true);
    gtk_layer_set_margin(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_BOTTOM,
                         150);
    gtk_widget_set_visible(GTK_WIDGET(self->win), false);

    // create volume OSD
    GtkBox *volume_osd = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_name(GTK_WIDGET(volume_osd), "osd-container");

    self->volume_icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("audio-volume-high-symbolic"));
    gtk_image_set_pixel_size(self->volume_icon, 32);

    self->volume_scale = GTK_SCALE(
        gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 1.0, 0.05));
    gtk_widget_set_hexpand(GTK_WIDGET(self->volume_scale), true);
    gtk_widget_set_sensitive(GTK_WIDGET(self->volume_scale), false);

    gtk_box_append(volume_osd, GTK_WIDGET(self->volume_icon));
    gtk_box_append(volume_osd, GTK_WIDGET(self->volume_scale));

    self->volume_revealer = GTK_REVEALER(gtk_revealer_new());
    gtk_widget_set_vexpand(GTK_WIDGET(self->volume_revealer), true);
    gtk_widget_set_hexpand(GTK_WIDGET(self->volume_revealer), true);
    gtk_revealer_set_transition_type(self->volume_revealer,
                                     GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
    gtk_revealer_set_transition_duration(self->volume_revealer, 400);

    // wire into "notify::child-revealed"
    g_signal_connect(self->volume_revealer, "notify::child-revealed",
                     G_CALLBACK(on_child_revealed), self);

    gtk_revealer_set_child(self->volume_revealer, GTK_WIDGET(volume_osd));

    // create brightness OSD
    GtkBox *brightness_osd =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_name(GTK_WIDGET(brightness_osd), "osd-container");

    self->bightness_icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("display-brightness-symbolic"));

    gtk_image_set_pixel_size(self->bightness_icon, 32);

    self->brightness_scale = GTK_SCALE(
        gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 1.0, 0.05));
    gtk_widget_set_hexpand(GTK_WIDGET(self->brightness_scale), true);
    gtk_widget_set_sensitive(GTK_WIDGET(self->brightness_scale), false);

    gtk_box_append(brightness_osd, GTK_WIDGET(self->bightness_icon));
    gtk_box_append(brightness_osd, GTK_WIDGET(self->brightness_scale));

    self->brightness_revealer = GTK_REVEALER(gtk_revealer_new());
    gtk_widget_set_vexpand(GTK_WIDGET(self->brightness_revealer), true);
    gtk_widget_set_hexpand(GTK_WIDGET(self->brightness_revealer), true);
    gtk_revealer_set_transition_type(self->brightness_revealer,
                                     GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
    gtk_revealer_set_transition_duration(self->brightness_revealer, 450);

    // wire into "notify::child-revealed"
    g_signal_connect(self->brightness_revealer, "notify::child-revealed",
                     G_CALLBACK(on_child_revealed), self);

    gtk_revealer_set_child(self->brightness_revealer,
                           GTK_WIDGET(brightness_osd));

    // wire into default sink volume changes
    WirePlumberService *wp = wire_plumber_service_get_global();
    g_signal_connect(wp, "default-sink-volume-changed",
                     G_CALLBACK(on_default_sink_changed), self);

    // wire into brightness changes
    BrightnessService *bs = brightness_service_get_global();
    g_signal_connect(bs, "brightness-changed",
                     G_CALLBACK(on_brightness_changed), self);

    // wire into quick settings mediator will show
    QuickSettingsMediator *qs = quick_settings_get_global_mediator();
    g_signal_connect(qs, "quick-settings-will-show",
                     G_CALLBACK(on_quick_settings_will_show), self);

    gtk_overlay_add_overlay(self->overlay, GTK_WIDGET(self->volume_revealer));
    gtk_overlay_add_overlay(self->overlay,
                            GTK_WIDGET(self->brightness_revealer));

    adw_window_set_content(self->win, GTK_WIDGET(self->overlay));
}

void osd_reinitialize(OSD *self) {
    // destroy signals
    WirePlumberService *wp = wire_plumber_service_get_global();
    g_signal_handlers_disconnect_by_func(wp, on_default_sink_changed, self);
    BrightnessService *bs = brightness_service_get_global();
    g_signal_handlers_disconnect_by_func(bs, on_brightness_changed, self);

    QuickSettingsMediator *qs = quick_settings_get_global_mediator();
    g_signal_handlers_disconnect_by_func(qs, on_quick_settings_will_show, self);

    osd_init_layout(self);
}

void osd_init(OSD *self) { osd_init_layout(self); }

void osd_activate(AdwApplication *app, gpointer user_data) {
    global = g_object_new(OSD_TYPE, NULL);
}
