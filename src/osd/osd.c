#include "./osd.h"

#include <adwaita.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

#include "../panel/quick_settings/quick_settings.h"
#include "../services/wireplumber_service.h"
#include "gtk/gtkrevealer.h"

static OSD *global = NULL;

enum signals { signals_n };

typedef struct _OSD {
    GObject parent_instance;
    AdwWindow *win;
    GtkBox *container;
    GtkRevealer *volume_revealer;
    GtkScale *volume_scale;
    GtkImage *volume_icon;
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
    if (self->timeout_id) {
        g_source_remove(self->timeout_id);
        self->timeout_id = 0;
    }
}

static void on_child_revealed(GObject *object, GParamSpec *pspec, OSD *self) {
    if (!gtk_revealer_get_child_revealed(self->volume_revealer))
        gtk_widget_set_visible(GTK_WIDGET(self->win), false);
}

static gboolean timed_dismiss(OSD *self) {
    if (!self->win) return false;
    gtk_revealer_set_reveal_child(self->volume_revealer, false);
    return false;
}

static void on_default_sink_changed(WirePlumberService *wp,
                                    WirePlumberServiceNode *sink, OSD *self) {
    g_debug("osd.c:on_default_sink_changed()");

    char *icon_name =
        wire_plumber_service_map_sink_vol_icon(sink->volume, sink->mute);

    // set icon
    gtk_image_set_from_icon_name(self->volume_icon, icon_name);

    // set audio scale
    gtk_range_set_value(GTK_RANGE(self->volume_scale), sink->volume);

    // if quick settings is currently displayed, don't show volume OSD, its
    // redunant
    QuickSettingsMediator *qs = quick_settings_get_global_mediator();
    if (quick_settings_mediator_qs_is_visible(qs)) return;

    // present window
    gtk_widget_set_visible(GTK_WIDGET(self->win), true);

    // reveal audio osd
    gtk_revealer_set_reveal_child(self->volume_revealer, true);

    // add a callback that runs in five seconds to dismiss the notification
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
}

void osd_init_layout(OSD *self) {
    self->win = ADW_WINDOW(adw_window_new());
    gtk_widget_set_size_request(GTK_WIDGET(self->win), 340, 50);

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
    // set icon size to 64
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
    gtk_revealer_set_transition_duration(self->volume_revealer, 450);

    // wire into "notify::child-revealed"
    g_signal_connect(self->volume_revealer, "notify::child-revealed",
                     G_CALLBACK(on_child_revealed), self);

    gtk_revealer_set_child(self->volume_revealer, GTK_WIDGET(volume_osd));

    // wire into default_sink_changed event
    WirePlumberService *wp = wire_plumber_service_get_global();
    g_signal_connect(wp, "default_sink_changed",
                     G_CALLBACK(on_default_sink_changed), self);

    // wire into quick settings mediator will show
    QuickSettingsMediator *qs = quick_settings_get_global_mediator();
    g_signal_connect(qs, "quick-settings-will-show",
                     G_CALLBACK(on_quick_settings_will_show), self);

    adw_window_set_content(self->win, GTK_WIDGET(self->volume_revealer));
}

void osd_reinitialize(OSD *self) {
    // destroy signals
    WirePlumberService *wp = wire_plumber_service_get_global();
    g_signal_handlers_disconnect_by_func(wp, on_default_sink_changed, self);

    QuickSettingsMediator *qs = quick_settings_get_global_mediator();
    g_signal_handlers_disconnect_by_func(qs, on_quick_settings_will_show, self);

    osd_init_layout(self);
}

void osd_init(OSD *self) { osd_init_layout(self); }

void osd_activate(AdwApplication *app, gpointer user_data) {
    global = g_object_new(OSD_TYPE, NULL);
}
