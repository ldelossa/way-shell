#include "quick_settings_header_mixer.h"

#include <adwaita.h>

#include "../../../../services/wireplumber_service.h"
#include "../../quick_settings_menu_widget.h"
#include "quick_settings_header_mixer_menu_option.h"

enum signals { signals_n };

typedef struct _QuickSettingsHeaderMixer {
    GObject parent_instance;
    QuickSettingsMenuWidget menu;
    GtkButton *mixer_button;
    GtkBox *container;
    GHashTable *node_to_widget;
} QuickSettingsHeaderMixer;
G_DEFINE_TYPE(QuickSettingsHeaderMixer, quick_settings_header_mixer,
              G_TYPE_OBJECT);

// stub out empty dispose, finalize, class_init, and init methods for this
// GObject.
static void quick_settings_header_mixer_dispose(GObject *gobject) {
    QuickSettingsHeaderMixer *self = QUICK_SETTINGS_HEADER_MIXER(gobject);

    // Chain-up
    G_OBJECT_CLASS(quick_settings_header_mixer_parent_class)->dispose(gobject);
};

static void quick_settings_header_mixer_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(quick_settings_header_mixer_parent_class)->finalize(gobject);
};

static void quick_settings_header_mixer_class_init(
    QuickSettingsHeaderMixerClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = quick_settings_header_mixer_dispose;
    object_class->finalize = quick_settings_header_mixer_finalize;
};

static void on_wire_plumber_service_database_changed(
    WirePlumberService *wps, GHashTable *db, QuickSettingsHeaderMixer *self) {
    g_debug(
        "quick_settings_header_mixer.c:on_wire_plumber_service_database_"
        "changed() called.");

    GtkWidget *child =
        gtk_widget_get_first_child(GTK_WIDGET(self->menu.options));
    while (child) {
        gtk_box_remove(self->menu.options, child);
        child = gtk_widget_get_first_child(GTK_WIDGET(self->menu.options));
    }

    // create sources
    GPtrArray *sources = wire_plumber_service_get_sources(wps);
    for (guint i = 0; i < sources->len; i++) {
        WirePlumberServiceNode *node = g_ptr_array_index(sources, i);

        QuickSettingsHeaderMixerMenuOption *option =
            g_object_new(QUICK_SETTINGS_HEADER_MIXER_MENU_OPTION_TYPE, NULL);

        quick_settings_header_mixer_menu_option_set_node(
            option, (WirePlumberServiceNodeHeader *)node);

        // add to menu.options container
        gtk_box_append(
            self->menu.options,
            quick_settings_header_mixer_menu_option_get_widget(option));
    }

    // create sinks
    GPtrArray *sinks = wire_plumber_service_get_sinks(wps);
    for (guint i = 0; i < sinks->len; i++) {
        WirePlumberServiceNode *node = g_ptr_array_index(sinks, i);

        QuickSettingsHeaderMixerMenuOption *option =
            g_object_new(QUICK_SETTINGS_HEADER_MIXER_MENU_OPTION_TYPE, NULL);

        quick_settings_header_mixer_menu_option_set_node(
            option, (WirePlumberServiceNodeHeader *)node);

        // add to menu.options container
        gtk_box_append(
            self->menu.options,
            quick_settings_header_mixer_menu_option_get_widget(option));
    }

    // create streams
    GPtrArray *streams = wire_plumber_service_get_streams(wps);
    for (guint i = 0; i < streams->len; i++) {
        WirePlumberServiceNode *node = g_ptr_array_index(streams, i);

        QuickSettingsHeaderMixerMenuOption *option =
            g_object_new(QUICK_SETTINGS_HEADER_MIXER_MENU_OPTION_TYPE, NULL);

        quick_settings_header_mixer_menu_option_set_node(
            option, (WirePlumberServiceNodeHeader *)node);

        // add to menu.options container
        gtk_box_append(
            self->menu.options,
            quick_settings_header_mixer_menu_option_get_widget(option));
    }
}

static void quick_settings_header_mixer_init_layout(
    QuickSettingsHeaderMixer *self) {
    // setup menu widget
    quick_settings_menu_widget_init(&self->menu, true);

    // have our menu request a bit more size up to 420 height
    gtk_widget_set_size_request(GTK_WIDGET(self->menu.container), -1, 420);
    // set vexpand on container
    gtk_widget_set_vexpand(GTK_WIDGET(self->menu.container), true);

    // set title to "Mixer"
    quick_settings_menu_widget_set_title(&self->menu, "Mixer");
    // set icon to "audio-speakers-symbolic"
    quick_settings_menu_widget_set_icon(&self->menu, "audio-speakers-symbolic");

    // create mixer button
    self->mixer_button =
        GTK_BUTTON(gtk_button_new_from_icon_name("audio-speakers-symbolic"));
    gtk_widget_add_css_class(GTK_WIDGET(self->mixer_button), "circular");

    WirePlumberService *wps = wire_plumber_service_get_global();

    on_wire_plumber_service_database_changed(
        wps, wire_plumber_service_get_db(wps), self);

    // connect to "database-changed"
    g_signal_connect(wps, "database-changed",
                     G_CALLBACK(on_wire_plumber_service_database_changed),
                     self);
};

static void quick_settings_header_mixer_init(QuickSettingsHeaderMixer *self) {
    quick_settings_header_mixer_init_layout(self);
};

void quick_settings_header_mixer_reinitialize(QuickSettingsHeaderMixer *self) {}

GtkWidget *quick_settings_header_mixer_get_menu_widget(
    QuickSettingsHeaderMixer *self) {
    return GTK_WIDGET(self->menu.container);
}

GtkButton *quick_settings_header_mixer_get_mixer_button(
    QuickSettingsHeaderMixer *self) {
    return self->mixer_button;
}