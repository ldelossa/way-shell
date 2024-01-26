#include "quick_settings_header_mixer_menu_option.h"

#include <adwaita.h>
#include <gio/gio.h>

#include "../../../../services/wireplumber_service.h"
#include "../../quick_settings_menu_widget.h"
#include "glibconfig.h"

GIcon *get_app_icon(const char *app_name) {
    GList *apps = g_app_info_get_all();
    GList *l;

    for (l = apps; l != NULL; l = l->next) {
        GAppInfo *app = G_APP_INFO(l->data);

        if (g_strcmp0(g_app_info_get_name(app), app_name) == 0) {
            GIcon *icon = g_app_info_get_icon(app);
            return icon;
        }
    }

    return NULL;
}

enum signals { signals_n };

typedef struct _QuickSettingsHeaderMixerMenuOption {
    GObject parent_instance;
    WirePlumberServiceNodeHeader *node;
    WirePlumberServiceNode *sink;
    WirePlumberServiceNode *source;
    WirePlumberServiceAudioStream *stream;
    GtkBox *container;
    GtkButton *button;
    GtkBox *button_contents;
    GtkLabel *node_name;
    GtkImage *icon;
    GtkImage *active_icon;
    GtkScale *volume_scale;
    GtkLabel *link;
    GtkRevealer *revealer;
    GtkBox *revealer_content;
} QuickSettingsHeaderMixerMenuOption;
G_DEFINE_TYPE(QuickSettingsHeaderMixerMenuOption,
              quick_settings_header_mixer_menu_option, G_TYPE_OBJECT);

// stub out empty dispose, finalize, class_init, and init methods for this
// GObject.
static void quick_settings_header_mixer_menu_option_dispose(GObject *gobject) {
    QuickSettingsHeaderMixerMenuOption *self =
        QUICK_SETTINGS_HEADER_MIXER_MENU_OPTION(gobject);

    g_debug(
        "quick_settings_header_mixer_menu_option.c:"
        "quick_settings_header_mixer_menu_option_dispose() called.");

    // Chain-up
    G_OBJECT_CLASS(quick_settings_header_mixer_menu_option_parent_class)
        ->dispose(gobject);
};

static void quick_settings_header_mixer_menu_option_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(quick_settings_header_mixer_menu_option_parent_class)
        ->finalize(gobject);
};

static void quick_settings_header_mixer_menu_option_class_init(
    QuickSettingsHeaderMixerMenuOptionClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = quick_settings_header_mixer_menu_option_dispose;
    object_class->finalize = quick_settings_header_mixer_menu_option_finalize;
};

static void on_option_click(GtkButton *button,
                            QuickSettingsHeaderMixerMenuOption *self) {
    g_debug(
        "quick_settings_header_mixer_menu_option.c:on_button_click() called.");
    gboolean revealed =
        gtk_revealer_get_reveal_child(GTK_REVEALER(self->revealer));
    gtk_revealer_set_reveal_child(self->revealer, !revealed);
    if (!revealed) {
        // grab focus of revealer_content
        gtk_widget_grab_focus(GTK_WIDGET(self->revealer_content));
    }
}

static void quick_settings_header_mixer_menu_option_init_layout(
    QuickSettingsHeaderMixerMenuOption *self) {
    // create main container
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->container),
                             "quick-settings-menu-option-mixer");

    // create main button
    self->button = GTK_BUTTON(gtk_button_new());

    // create button contents
    self->button_contents = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

    self->icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("audio-speakers-symbolic"));

    self->active_icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("media-record-symbolic"));

    self->node_name = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(self->node_name, 0);
    gtk_label_set_max_width_chars(self->node_name, 120);
    gtk_label_set_ellipsize(self->node_name, PANGO_ELLIPSIZE_END);

    // append icon, node_name, active icon to button_contents
    gtk_box_append(self->button_contents, GTK_WIDGET(self->active_icon));
    gtk_box_append(self->button_contents, GTK_WIDGET(self->icon));
    gtk_box_append(self->button_contents, GTK_WIDGET(self->node_name));

    // create volume scale
    self->volume_scale = GTK_SCALE(
        gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 1.0, .5));
    gtk_widget_set_hexpand(GTK_WIDGET(self->volume_scale), true);

    // add button_contents as button child
    gtk_button_set_child(self->button, GTK_WIDGET(self->button_contents));

    // add button to container
    gtk_box_append(self->container, GTK_WIDGET(self->button));

    // add revealer
    self->revealer = GTK_REVEALER(gtk_revealer_new());
    self->revealer_content = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_can_focus(GTK_WIDGET(self->revealer_content), true);
    gtk_revealer_set_child(self->revealer, GTK_WIDGET(self->revealer_content));

    // append revealer to container
    gtk_box_append(self->container, GTK_WIDGET(self->revealer));

    g_signal_connect(self->button, "clicked", G_CALLBACK(on_option_click),
                     self);
};

static void quick_settings_header_mixer_menu_option_init(
    QuickSettingsHeaderMixerMenuOption *self) {
    quick_settings_header_mixer_menu_option_init_layout(self);
};

static void set_sink(QuickSettingsHeaderMixerMenuOption *self,
                     WirePlumberServiceNode *node) {
    self->sink = node;
    gchar *icon =
        wire_plumber_service_map_sink_vol_icon(node->volume, node->mute);
    // set icon
    gtk_image_set_from_icon_name(self->icon, icon);
    // set name
    if (node->nick_name) {
        gtk_label_set_text(self->node_name, node->nick_name);
    } else {
        gtk_label_set_text(self->node_name, node->name);
    }
    // set tooltip to node_name
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->button), node->name);

    // set scale to node's volume
    gtk_range_set_value(GTK_RANGE(self->volume_scale), node->volume);

    // set scale as revealer's content
    gtk_box_append(self->revealer_content, GTK_WIDGET(self->volume_scale));

    if (node->state == WP_NODE_STATE_RUNNING) {
        gtk_widget_add_css_class(GTK_WIDGET(self->active_icon),
                                 "active-icon-activated");
    } else {
        gtk_widget_remove_css_class(GTK_WIDGET(self->active_icon),
                                    "active-icon-activated");
    }
}

static void set_source(QuickSettingsHeaderMixerMenuOption *self,
                       WirePlumberServiceNode *node) {
    self->source = node;
    gchar *icon =
        wire_plumber_service_map_source_vol_icon(node->volume, node->mute);
    // set icon
    gtk_image_set_from_icon_name(self->icon, icon);
    // set name
    if (node->nick_name) {
        gtk_label_set_text(self->node_name, node->nick_name);
    } else {
        gtk_label_set_text(self->node_name, node->name);
    }
    // set tooltip to node_name
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->button), node->name);

    // set scale to node's volume
    gtk_range_set_value(GTK_RANGE(self->volume_scale), node->volume);

    // set scale as revealer's content
    gtk_box_append(self->revealer_content, GTK_WIDGET(self->volume_scale));

    // check state and if running set active css on active_icon
    if (node->state == WP_NODE_STATE_RUNNING) {
        gtk_widget_add_css_class(GTK_WIDGET(self->active_icon),
                                 "active-icon-activated");
    } else {
        gtk_widget_remove_css_class(GTK_WIDGET(self->active_icon),
                                    "active-icon-activated");
    }
}

GtkButton *link_button_new(WirePlumberServiceNodeHeader *header) {
    WirePlumberServiceNode *node = (WirePlumberServiceNode *)header;

    // create button which reveals the link
    GtkButton *link_button = GTK_BUTTON(gtk_button_new());
    gtk_widget_set_tooltip_text(GTK_WIDGET(link_button), node->name);

    // create button content container
    GtkBox *button_content =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(link_button), "mixer-link");

    // create link icon, the hotspot icon looks like a chain link which
    // is good for this purpose.
    GtkImage *icon = GTK_IMAGE(
        gtk_image_new_from_icon_name("network-wireless-hotspot-symbolic"));

    // copy the name of the link
    char *name = NULL;
    if (node->nick_name) {
        name = g_strdup(node->nick_name);
    } else {
        name = g_strdup(node->name);
    }

    // add label of link name
    GtkLabel *label = GTK_LABEL(gtk_label_new(name));
    gtk_label_set_max_width_chars(label, 120);
    gtk_label_set_ellipsize(label, PANGO_ELLIPSIZE_END);

    // append icon and label to button contents and set contents as
    // button's child
    gtk_box_append(button_content, GTK_WIDGET(icon));
    gtk_box_append(button_content, GTK_WIDGET(label));
    gtk_button_set_child(link_button, GTK_WIDGET(button_content));

    return link_button;
}

static void set_stream(QuickSettingsHeaderMixerMenuOption *self,
                       WirePlumberServiceAudioStream *node) {
    self->stream = node;

    // we want to find where the audio stream is linked to and display this.
    WirePlumberServiceNodeHeader *header = NULL;
    GHashTable *db =
        wire_plumber_service_get_db(wire_plumber_service_get_global());
    GPtrArray *links =
        wire_plumber_service_get_links(wire_plumber_service_get_global());

    GHashTable *link_nodes = g_hash_table_new(g_direct_hash, g_direct_equal);

    // search through links and record which input and output nodes are linked
    // to this stream.
    // we will "roll up" multiple links to the same input/output ids for sake
    // if displaying to the user.
    for (int i = 0; i < links->len; i++) {
        WirePlumberServiceLink *link = g_ptr_array_index(links, i);

        // if we have an output stream, search for links which use us as their
        // input.
        if (node->type == WIRE_PLUMBER_SERVICE_TYPE_OUTPUT_AUDIO_STREAM)
            if (link->output_node_id == node->id) {
                if (g_hash_table_contains(
                        link_nodes, GUINT_TO_POINTER(link->output_node_id)))
                    continue;
                g_hash_table_add(link_nodes,
                                 GUINT_TO_POINTER(link->input_node_id));
            }

        // if we have an input stream search for links which use us as their
        // input.
        if (node->type == WIRE_PLUMBER_SERVICE_TYPE_INPUT_AUDIO_STREAM)
            if (link->input_node_id == node->id) {
                if (g_hash_table_contains(
                        link_nodes, GUINT_TO_POINTER(link->output_node_id)))
                    continue;
                g_hash_table_add(link_nodes,
                                 GUINT_TO_POINTER(link->output_node_id));
            }
    }

    // iterate over link nodes and create a button for each
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, link_nodes);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        header = g_hash_table_lookup(db, key);
        gtk_box_append(self->revealer_content,
                       GTK_WIDGET(link_button_new(header)));
    }

    // lets see if we can find a desktop icon for the app with the stream
    // name.
    GIcon *icon = get_app_icon(node->app_name);
    if (icon)
        gtk_image_set_from_gicon(self->icon, icon);
    else
        gtk_image_set_from_icon_name(self->icon, "audio-speakers-symbolic");

    // set name based on stream direction
    if (node->type == WIRE_PLUMBER_SERVICE_TYPE_INPUT_AUDIO_STREAM) {
        gtk_label_set_text(
            self->node_name,
            g_strdup_printf("%s (%s)", node->app_name, "Input"));
    } else {
        gtk_label_set_text(
            self->node_name,
            g_strdup_printf("%s (%s)", node->app_name, "Output"));
    }
    // set tooltip to node_name
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->button), node->app_name);

    if (node->state == WP_NODE_STATE_RUNNING) {
        gtk_widget_add_css_class(GTK_WIDGET(self->active_icon),
                                 "active-icon-activated");
    } else {
        gtk_widget_remove_css_class(GTK_WIDGET(self->active_icon),
                                    "active-icon-activated");
    }
}

void quick_settings_header_mixer_menu_option_set_node(
    QuickSettingsHeaderMixerMenuOption *self,
    WirePlumberServiceNodeHeader *node) {
    g_debug(
        "quick_settings_header_mixer_menu_option.c:"
        "quick_settings_header_mixer_menu_option_set_node() called.");

    if (node->type == WIRE_PLUMBER_SERVICE_TYPE_SINK) {
        set_sink(self, (WirePlumberServiceNode *)node);
    }
    if (node->type == WIRE_PLUMBER_SERVICE_TYPE_SOURCE) {
        set_source(self, (WirePlumberServiceNode *)node);
    }
    if (node->type == WIRE_PLUMBER_SERVICE_TYPE_INPUT_AUDIO_STREAM ||
        node->type == WIRE_PLUMBER_SERVICE_TYPE_OUTPUT_AUDIO_STREAM) {
        set_stream(self, (WirePlumberServiceAudioStream *)node);
    }
}

GtkWidget *quick_settings_header_mixer_menu_option_get_widget(
    QuickSettingsHeaderMixerMenuOption *self) {
    return GTK_WIDGET(self->container);
}