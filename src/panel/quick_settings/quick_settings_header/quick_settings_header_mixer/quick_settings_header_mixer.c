#include "quick_settings_header_mixer.h"

#include <adwaita.h>

#include "../../../../services/wireplumber_service.h"
#include "../../quick_settings.h"
#include "../../quick_settings_menu_widget.h"
#include "quick_settings_header_mixer_menu_option.h"

enum signals { signals_n };

typedef struct _QuickSettingsHeaderMixer {
    GObject parent_instance;
    QuickSettingsMenuWidget menu;
    GtkButton *mixer_button;
    GtkBox *container;
    GHashTable *nodes;
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

    GPtrArray *to_remove = g_ptr_array_new();

    // prune all streams from our inventory, we want to refresh these
    // everytime this event is called.
    GtkWidget *child =
        gtk_widget_get_first_child(GTK_WIDGET(self->menu.options));
    while (child) {
        QuickSettingsHeaderMixerMenuOption *option =
            g_object_get_data(G_OBJECT(child), "option");
        WirePlumberServiceNodeHeader *node =
            quick_settings_header_mixer_menu_option_get_node(option);
        if (node->type == WIRE_PLUMBER_SERVICE_TYPE_INPUT_AUDIO_STREAM ||
            node->type == WIRE_PLUMBER_SERVICE_TYPE_OUTPUT_AUDIO_STREAM) {
            g_ptr_array_add(to_remove, option);
        }
        child = gtk_widget_get_next_sibling(child);
    }

    for (guint i = 0; i < to_remove->len; i++) {
        QuickSettingsHeaderMixerMenuOption *option =
            g_ptr_array_index(to_remove, i);
        gtk_box_remove(
            self->menu.options,
            quick_settings_header_mixer_menu_option_get_widget(option));
    }

    // set to_remove size to zero
    g_ptr_array_set_size(to_remove, 0);

    // create streams, we always want to recreate these since link info is
    // refreshed in this event.
    GPtrArray *streams = wire_plumber_service_get_streams(wps);
    for (guint i = 0; i < streams->len; i++) {
        WirePlumberServiceNodeHeader *header = g_ptr_array_index(streams, i);

        QuickSettingsHeaderMixerMenuOption *option =
            g_object_new(QUICK_SETTINGS_HEADER_MIXER_MENU_OPTION_TYPE, NULL);

        quick_settings_header_mixer_menu_option_set_node(option, header);

        // add to menu.options container
        gtk_box_append(
            self->menu.options,
            quick_settings_header_mixer_menu_option_get_widget(option));
    }

    // create sources
    GPtrArray *sources = wire_plumber_service_get_sources(wps);
    for (guint i = 0; i < sources->len; i++) {
        WirePlumberServiceNodeHeader *header = g_ptr_array_index(sources, i);

        if (g_hash_table_contains(self->nodes, GUINT_TO_POINTER(header->id))) {
            continue;
        }

        QuickSettingsHeaderMixerMenuOption *option =
            g_object_new(QUICK_SETTINGS_HEADER_MIXER_MENU_OPTION_TYPE, NULL);

        quick_settings_header_mixer_menu_option_set_node(option, header);

        // add to menu.options container
        gtk_box_append(
            self->menu.options,
            quick_settings_header_mixer_menu_option_get_widget(option));

        g_hash_table_add(self->nodes, GUINT_TO_POINTER(header->id));
    }

    // create sinks
    GPtrArray *sinks = wire_plumber_service_get_sinks(wps);
    for (guint i = 0; i < sinks->len; i++) {
        WirePlumberServiceNodeHeader *header = g_ptr_array_index(sinks, i);

        if (g_hash_table_contains(self->nodes, GUINT_TO_POINTER(header->id))) {
            continue;
        }

        QuickSettingsHeaderMixerMenuOption *option =
            g_object_new(QUICK_SETTINGS_HEADER_MIXER_MENU_OPTION_TYPE, NULL);

        quick_settings_header_mixer_menu_option_set_node(option, header);

        // add to menu.options container
        gtk_box_append(
            self->menu.options,
            quick_settings_header_mixer_menu_option_get_widget(option));

        g_hash_table_add(self->nodes, GUINT_TO_POINTER(header->id));
    }

    // prune menu's options if they are not present in db
    child = gtk_widget_get_first_child(GTK_WIDGET(self->menu.options));
    while (child) {
        // get options structure pointer from widget
        QuickSettingsHeaderMixerMenuOption *option =
            g_object_get_data(G_OBJECT(child), "option");

        WirePlumberServiceNodeHeader *node =
            quick_settings_header_mixer_menu_option_get_node(option);

        if (!g_hash_table_contains(db, GUINT_TO_POINTER(node->id))) {
            g_ptr_array_add(to_remove, option);
            g_hash_table_remove(self->nodes, GUINT_TO_POINTER(node->id));
        }
        child = gtk_widget_get_next_sibling(child);
    }

    // remove all options in to_remove array
    for (guint i = 0; i < to_remove->len; i++) {
        QuickSettingsHeaderMixerMenuOption *option =
            g_ptr_array_index(to_remove, i);
        gtk_box_remove(
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
    self->nodes = g_hash_table_new(g_direct_hash, g_direct_equal);
};

void quick_settings_header_mixer_reinitialize(QuickSettingsHeaderMixer *self) {
    // kill signals
    WirePlumberService *wps = wire_plumber_service_get_global();
    g_signal_handlers_disconnect_by_func(
        wps, G_CALLBACK(on_wire_plumber_service_database_changed), self);

    // empty node's hash table
    g_hash_table_remove_all(self->nodes);

    // reinit layout
    quick_settings_header_mixer_init_layout(self);
}

GtkWidget *quick_settings_header_mixer_get_menu_widget(
    QuickSettingsHeaderMixer *self) {
    return GTK_WIDGET(self->menu.container);
}

GtkButton *quick_settings_header_mixer_get_mixer_button(
    QuickSettingsHeaderMixer *self) {
    return self->mixer_button;
}