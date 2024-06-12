#include "wireplumber_service.h"

#include <adwaita.h>
#include <pipewire/keys.h>
#include <pulse/context.h>
#include <pulse/glib-mainloop.h>
#include <pulse/introspect.h>
#include <pulse/pulseaudio.h>
#include <wireplumber-0.5/wp/component-loader.h>
#include <wireplumber-0.5/wp/wp.h>

#include "wp/core.h"
#include "wp/global-proxy.h"
#include "wp/iterator.h"
#include "wp/link.h"
#include "wp/node.h"
#include "wp/object-interest.h"
#include "wp/object-manager.h"
#include "wp/proxy-interfaces.h"
#include "wp/proxy.h"

enum signals {
    // a particular node's detail has changed, a signal with the pointer to the
    // node is emitted.
    node_changed,
    // an object has been added or removed from the object database, a signal
    // with the GHashTable database is emitted.
    database_changed,
    // the default sink has changed, a signal with the default sink is emitted.
    default_sink_changed,
    // the default sink's volume has changed, this is helpful since the above
    // event fires on more then just volume changes (like link changes).
    default_sink_volume_changed,
    // the default sink has changed, a signal with the default source is
    // emitted.
    default_source_changed,
    // the default source's volume has changed, this is helpful since the above
    // event fires on more then just volume changes (like link changes).
    default_source_volume_changed,
    // a signal emitted with a boolean informing if any microphone is currently
    // listening.
    microphone_active,
    signals_n
};

static WirePlumberService *global = NULL;

struct _WirePlumberService {
    GObject parent_instance;
    WpCore *core;
    WpObjectManager *om;
    WpPlugin *default_nodes_api;
    WpPlugin *mixer_api;
    WirePlumberServiceNode *default_sink;
    WirePlumberServiceNode *default_source;
    guint32 default_sink_id;
    guint32 default_source_id;

    // we still need to use pulse audio because WirePlumber alone does not know
    // how to restore stream sink/sources on flap.
    struct pa_context *pa_ctx;
    struct pa_glib_mainloop *pa_loop;
    struct pa_mainloop_api *pa_api;

    GPtrArray *sinks;
    GPtrArray *sources;
    GPtrArray *streams;
    GPtrArray *links;
    GHashTable *db;
    guint32 pending_output_stream;
    guint32 pending_sink;
    guint32 pending_input_stream;
    guint32 pending_source;
    int pending_plugins;
};

static guint service_signals[signals_n] = {0};
G_DEFINE_TYPE(WirePlumberService, wire_plumber_service, G_TYPE_OBJECT);

// stub out dispose, finalize, class init and init methods for this GObject
// class.
static void wire_plumber_service_dispose(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(wire_plumber_service_parent_class)->dispose(gobject);
};

static void wire_plumber_service_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(wire_plumber_service_parent_class)->finalize(gobject);
};

static void wire_plumber_service_class_init(WirePlumberServiceClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = wire_plumber_service_dispose;
    object_class->finalize = wire_plumber_service_finalize;

    // define 'node_changed' signal
    service_signals[node_changed] = g_signal_new(
        "node-changed", G_TYPE_FROM_CLASS(object_class), G_SIGNAL_RUN_FIRST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);

    // define 'database_changed' signal
    service_signals[database_changed] = g_signal_new(
        "database-changed", G_TYPE_FROM_CLASS(object_class), G_SIGNAL_RUN_FIRST,
        0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);

    // define 'default_sink_changed' signal
    service_signals[default_sink_changed] =
        g_signal_new("default-sink-changed", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
                     G_TYPE_POINTER);

    service_signals[default_sink_volume_changed] =
        g_signal_new("default-sink-volume-changed",
                     G_TYPE_FROM_CLASS(object_class), G_SIGNAL_RUN_FIRST, 0,
                     NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);

    // define 'default_source_changed' signal
    service_signals[default_source_changed] =
        g_signal_new("default-source-changed", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
                     G_TYPE_POINTER);

    service_signals[default_source_volume_changed] =
        g_signal_new("default-source-volume-changed",
                     G_TYPE_FROM_CLASS(object_class), G_SIGNAL_RUN_FIRST, 0,
                     NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);

    // define microphone-active signal
    service_signals[microphone_active] =
        g_signal_new("microphone-active", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
                     G_TYPE_BOOLEAN);
};

gboolean wire_plumber_service_microphone_active(WirePlumberService *self) {
    gboolean active = false;
    for (int i = 0; i < self->sources->len; i++) {
        WirePlumberServiceNode *node = g_ptr_array_index(self->sources, i);
        if (node->state == WP_NODE_STATE_RUNNING) {
            active = true;
            break;
        }
    }
    return active;
}

static void wire_plumber_service_clean_audio_node(
    WirePlumberServiceAudioStream *node) {
    g_free((void *)node->media_class);
    node->media_class = NULL;
    g_free((void *)node->name);
    node->name = NULL;
    g_free((void *)node->app_name);
    node->app_name = NULL;
}

static void wire_plumber_service_fill_audio_stream(
    WirePlumberServiceAudioStream *node, WpGlobalProxy *proxy,
    WirePlumberService *self) {
    GVariant *mixer_values = NULL;

    // clean the node of any alloc'd data before we reset the fields.
    wire_plumber_service_clean_audio_node(node);

    // fill in id and state fields
    node->id = wp_proxy_get_bound_id(WP_PROXY(proxy));
    node->state = wp_node_get_state(WP_NODE(proxy), NULL);

    // fill in audio details from mixer api
    g_signal_emit_by_name(self->mixer_api, "get-volume", node->id,
                          &mixer_values);
    if (!mixer_values) return;

    g_variant_lookup(mixer_values, "volume", "d", &node->volume);
    g_variant_lookup(mixer_values, "mute", "b", &node->mute);
    g_variant_lookup(mixer_values, "step", "b", &node->step);
    g_variant_lookup(mixer_values, "base", "d", &node->base);

    // when we receive the volume its a percentage (0.0-1.0) cubed.
    // for example, when you do a `wpctl set-volume [node] 0.2` the volume we
    // pickup from the API is (0.2^3) = 0.000008
    //
    // its easier to work with decimals between 0.0-1.0 so convert this value
    // to the linear scale by taking the cubed root.
    node->volume = volume_from_linear(node->volume, SCALE_CUBIC);

    node->media_class = g_strdup(wp_pipewire_object_get_property(
        WP_PIPEWIRE_OBJECT(proxy), PW_KEY_MEDIA_CLASS));
    node->name = g_strdup(wp_pipewire_object_get_property(
        WP_PIPEWIRE_OBJECT(proxy), PW_KEY_NODE_DESCRIPTION));
    node->app_name = g_strdup(wp_pipewire_object_get_property(
        WP_PIPEWIRE_OBJECT(proxy), PW_KEY_APP_NAME));
    node->media_name = g_strdup(wp_pipewire_object_get_property(
        WP_PIPEWIRE_OBJECT(proxy), PW_KEY_MEDIA_NAME));

    // check media type and set actual type
    if (g_strcmp0(node->media_class, "Stream/Output/Audio") == 0) {
        node->type = WIRE_PLUMBER_SERVICE_TYPE_OUTPUT_AUDIO_STREAM;
    }
    if (g_strcmp0(node->media_class, "Stream/Input/Audio") == 0) {
        node->type = WIRE_PLUMBER_SERVICE_TYPE_INPUT_AUDIO_STREAM;
    }
}

static void wire_plumber_service_clean_source_sink_node(
    WirePlumberServiceNode *node) {
    g_free((void *)node->media_class);
    node->media_class = NULL;
    g_free((void *)node->name);
    node->name = NULL;
    g_free((void *)node->nick_name);
    node->nick_name = NULL;
}

static void wire_plumber_service_fill_node(WirePlumberServiceNode *node,
                                           WpGlobalProxy *proxy,
                                           WirePlumberService *self) {
    GVariant *mixer_values = NULL;

    // clean the node of any alloc'd data before we reset the fields.
    wire_plumber_service_clean_source_sink_node(node);

    // fill in id and state fields
    node->id = wp_proxy_get_bound_id(WP_PROXY(proxy));
    node->state = wp_node_get_state(WP_NODE(proxy), NULL);

    // fill in audio details from mixer api
    g_signal_emit_by_name(self->mixer_api, "get-volume", node->id,
                          &mixer_values);
    g_variant_lookup(mixer_values, "volume", "d", &node->volume);
    g_variant_lookup(mixer_values, "mute", "b", &node->mute);
    g_variant_lookup(mixer_values, "step", "b", &node->step);
    g_variant_lookup(mixer_values, "base", "d", &node->base);

    // when we receive the volume its a percentage (0.0-1.0) cubed.
    // for example, when you do a `wpctl set-volume [node] 0.2` the volume we
    // pickup from the API is (0.2^3) = 0.000008
    //
    // its easier to work with decimals between 0.0-1.0 so convert this value
    // to the linear scale by taking the cubed root.
    node->volume = volume_from_linear(node->volume, SCALE_CUBIC);

    node->media_class = g_strdup(wp_pipewire_object_get_property(
        WP_PIPEWIRE_OBJECT(proxy), PW_KEY_MEDIA_CLASS));
    node->name = g_strdup(wp_pipewire_object_get_property(
        WP_PIPEWIRE_OBJECT(proxy), PW_KEY_NODE_DESCRIPTION));
    node->nick_name = g_strdup(wp_pipewire_object_get_property(
        WP_PIPEWIRE_OBJECT(proxy), PW_KEY_NODE_NICK));
    node->proper_name = g_strdup(wp_pipewire_object_get_property(
        WP_PIPEWIRE_OBJECT(proxy), PW_KEY_NODE_NAME));

    // check media type and set actual type
    if (g_strcmp0(node->media_class, "Audio/Sink") == 0) {
        node->type = WIRE_PLUMBER_SERVICE_TYPE_SINK;
    }
    if (g_strcmp0(node->media_class, "Audio/Source") == 0) {
        node->type = WIRE_PLUMBER_SERVICE_TYPE_SOURCE;
    }

    // debug volume
    g_debug(
        "wireplumber_service.c:wire_plumber_service_fill_node() id: %d, name: "
        "%s, media_class: %s, volume: %f, mute: %d, step: %f, base: %f, state: "
        "%d",
        node->id, node->name, node->media_class, node->volume, node->mute,
        node->step, node->base, node->state);
}

static void wire_plumber_service_fill_link(WirePlumberServiceLink *link,
                                           WpGlobalProxy *proxy,
                                           WirePlumberService *self) {
    link->id = wp_proxy_get_bound_id(WP_PROXY(proxy));

    const char *input_node = NULL;
    const char *input_port = NULL;
    const char *output_node = NULL;
    const char *output_port = NULL;

    input_node = wp_pipewire_object_get_property(WP_PIPEWIRE_OBJECT(proxy),
                                                 PW_KEY_LINK_INPUT_NODE);
    input_port = wp_pipewire_object_get_property(WP_PIPEWIRE_OBJECT(proxy),
                                                 PW_KEY_LINK_INPUT_PORT);

    output_node = wp_pipewire_object_get_property(WP_PIPEWIRE_OBJECT(proxy),
                                                  PW_KEY_LINK_OUTPUT_NODE);
    output_port = wp_pipewire_object_get_property(WP_PIPEWIRE_OBJECT(proxy),
                                                  PW_KEY_LINK_OUTPUT_PORT);

    if (input_node) link->input_node = g_ascii_strtoull(input_node, NULL, 10);
    if (input_port) link->input_port = g_ascii_strtoull(input_port, NULL, 10);
    if (output_node)
        link->output_node = g_ascii_strtoull(output_node, NULL, 10);
    if (output_port)
        link->output_port = g_ascii_strtoull(output_port, NULL, 10);

    link->type = WIRE_PLUMBER_SERVICE_TYPE_LINK;
};

WirePlumberServiceLink *wire_plumber_service_link_new(
    WpGlobalProxy *proxy, WirePlumberService *self) {
    g_debug("wireplumber_service.c:wire_plumber_service_node_link() called");

    WirePlumberServiceLink *link = g_malloc0(sizeof(WirePlumberServiceLink));
    wire_plumber_service_fill_link(link, proxy, self);
    link->type = WIRE_PLUMBER_SERVICE_TYPE_LINK;

    return link;
}

static void on_mixer_changed(void *_, guint id, WirePlumberService *self) {
    g_debug("wireplumber_service.c:on_mixer_changed() called");

    WpGlobalProxy *pw = wp_object_manager_lookup(self->om, WP_TYPE_GLOBAL_PROXY,
                                                 WP_CONSTRAINT_TYPE_G_PROPERTY,
                                                 "bound-id", "=u", id, NULL);
    if (!pw) {
        g_debug("wireplumber_service.c:on_node_property_change() pw not found");
        return;
    }

    // find node in db
    WirePlumberServiceNodeHeader *header =
        g_hash_table_lookup(self->db, GUINT_TO_POINTER(id));

    if (!header) {
        g_debug(
            "wireplumber_service.c:on_node_property_change() node not found %d",
            id);
        return;
    }

    if (header->type == WIRE_PLUMBER_SERVICE_TYPE_SINK ||
        header->type == WIRE_PLUMBER_SERVICE_TYPE_SOURCE) {
        WirePlumberServiceNode *node = (WirePlumberServiceNode *)header;

        double old_volume = node->volume;
        gboolean old_mute = node->mute;

        g_debug(
            "wireplumber_service.c:on_node_property_change() id: %d, name: %s, "
            "media_class: %s, volume: %f, mute: %d, step: %f, base: %f, state: "
            "%d",
            node->id, node->name, node->media_class, node->volume, node->mute,
            node->step, node->base, node->state);

        // update node
        wire_plumber_service_fill_node((WirePlumberServiceNode *)node,
                                       WP_GLOBAL_PROXY(pw), self);

        if (node == self->default_sink) {
            g_signal_emit(self, service_signals[default_sink_changed], 0, node);
            if (node->volume != old_volume || node->mute != old_mute) {
                g_signal_emit(self,
                              service_signals[default_sink_volume_changed], 0,
                              node);
            }
        }

        if (node == self->default_source) {
            g_signal_emit(self, service_signals[default_source_changed], 0,
                          node);
            if (node->volume != old_volume || node->mute != old_mute) {
                g_signal_emit(self,
                              service_signals[default_source_volume_changed], 0,
                              node);
            }
        }
    }

    if (header->type == WIRE_PLUMBER_SERVICE_TYPE_INPUT_AUDIO_STREAM ||
        header->type == WIRE_PLUMBER_SERVICE_TYPE_OUTPUT_AUDIO_STREAM) {
        WirePlumberServiceAudioStream *node =
            (WirePlumberServiceAudioStream *)header;

        g_debug(
            "wireplumber_service.c:on_node_property_change() id: %d, name: %s, "
            "app_name: %s"
            "media_class: %s, volume: %f, mute: %d, step: %f, base: %f, state: "
            "%d",
            node->id, node->name, node->app_name, node->media_class,
            node->volume, node->mute, node->step, node->base, node->state);

        wire_plumber_service_fill_audio_stream(node, WP_GLOBAL_PROXY(pw), self);
    }

    g_signal_emit(self, service_signals[node_changed], 0, header);

    if (wire_plumber_service_microphone_active(self))
        g_signal_emit(self, service_signals[microphone_active], 0, true);
    else
        g_signal_emit(self, service_signals[microphone_active], 0, false);
}

static void on_state_change(WpNode *node, WpNodeState old_state,
                            WpNodeState new_state, WirePlumberService *self) {
    g_debug("wireplumber_service.c:on_state_change() called");
    guint32 id = wp_proxy_get_bound_id(WP_PROXY(node));
    on_mixer_changed(NULL, id, self);
}

static void wire_plumber_service_prune_db(WirePlumberService *self) {
    g_auto(GValue) value = G_VALUE_INIT;
    WpIterator *it = NULL;
    GHashTable *set = g_hash_table_new(g_direct_hash, g_direct_equal);

    // create set
    it = wp_object_manager_new_iterator(self->om);

    for (; wp_iterator_next(it, &value); g_value_unset(&value)) {
        GObject *obj = g_value_get_object(&value);
        guint32 id = wp_proxy_get_bound_id(WP_PROXY(obj));
        g_hash_table_add(set, GUINT_TO_POINTER(id));
    }

    GHashTableIter iter;
    gpointer key, val;
    GList *keys_to_remove = NULL;
    GList *i;

    g_hash_table_iter_init(&iter, self->db);
    while (g_hash_table_iter_next(&iter, &key, &val)) {
        if (!g_hash_table_contains(set, key)) {
            keys_to_remove = g_list_prepend(keys_to_remove, key);
        }
    }
    for (i = keys_to_remove; i != NULL; i = i->next) {
        g_hash_table_remove(self->db, i->data);
    }
    g_list_free(keys_to_remove);

    // prune sinks array
    if (self->sinks->len > 0)
        for (int i = self->sinks->len - 1; i >= 0; i--) {
            WirePlumberServiceNode *node = g_ptr_array_index(self->sinks, i);
            if (!g_hash_table_contains(set, GUINT_TO_POINTER(node->id))) {
                g_ptr_array_remove_index(self->sinks, i);
                wire_plumber_service_clean_source_sink_node(node);
                g_free(node);
            }
        }

    // prune source array
    if (self->sinks->len > 0)
        for (int i = self->sources->len - 1; i >= 0; i--) {
            WirePlumberServiceNode *node = g_ptr_array_index(self->sources, i);
            if (!g_hash_table_contains(set, GUINT_TO_POINTER(node->id))) {
                g_ptr_array_remove_index(self->sources, i);
                wire_plumber_service_clean_source_sink_node(node);
                g_free(node);
            }
        }

    // prune streams
    if (self->streams->len > 0)
        for (int i = self->streams->len - 1; i >= 0; i--) {
            WirePlumberServiceAudioStream *stream =
                g_ptr_array_index(self->streams, i);
            if (!g_hash_table_contains(set, GUINT_TO_POINTER(stream->id))) {
                g_ptr_array_remove_index(self->streams, i);
                wire_plumber_service_clean_audio_node(stream);
                g_free(stream);
            }
        }

    // prune links
    if (self->links->len > 0)
        for (int i = self->links->len - 1; i >= 0; i--) {
            WirePlumberServiceLink *link = g_ptr_array_index(self->links, i);
            if (!g_hash_table_contains(set, GUINT_TO_POINTER(link->id))) {
                g_ptr_array_remove_index(self->links, i);
                g_free(link);
            }
        }
}

WirePlumberServiceNode *wire_plumber_service_node_new(
    WpGlobalProxy *proxy, WirePlumberService *self) {
    g_debug("wireplumber_service.c:set_default_source() called");

    WirePlumberServiceNode *node = g_malloc0(sizeof(WirePlumberServiceNode));

    wire_plumber_service_fill_node(node, proxy, self);

    return node;
}

WirePlumberServiceAudioStream *wire_plumber_service_audio_stream_new(
    WpGlobalProxy *proxy, WirePlumberService *self) {
    g_debug("wireplumber_service.c:set_default_source() called");

    WirePlumberServiceAudioStream *node =
        g_malloc0(sizeof(WirePlumberServiceAudioStream));

    wire_plumber_service_fill_audio_stream(node, proxy, self);

    return node;
}

static void on_object_manager_change_get_source_sinks(WpObjectManager *om,
                                                      WirePlumberService *self,
                                                      char *media_class) {
    g_auto(GValue) value = G_VALUE_INIT;
    WpIterator *it = NULL;
    GPtrArray *node_array = NULL;
    WirePlumberServiceNode **default_node;
    guint32 default_node_id = 0;

    g_debug("wireplumber_service.c:on_object_manager_change() called");

    if (g_strcmp0(media_class, "Audio/Sink") == 0) {
        node_array = self->sinks;
        default_node = &self->default_sink;
        default_node_id = self->default_sink_id;
    }

    if (g_strcmp0(media_class, "Audio/Source") == 0) {
        node_array = self->sources;
        default_node = &self->default_source;
        default_node_id = self->default_source_id;
    }

    it = wp_object_manager_new_filtered_iterator(
        self->om, WP_TYPE_NODE, WP_CONSTRAINT_TYPE_PW_PROPERTY,
        PW_KEY_MEDIA_CLASS, "=s", media_class, NULL);

    for (; wp_iterator_next(it, &value); g_value_unset(&value)) {
        GObject *obj = g_value_get_object(&value);
        guint32 id = wp_proxy_get_bound_id(WP_PROXY(obj));
        WirePlumberServiceNode *node = NULL;

        node = g_hash_table_lookup(self->db, GUINT_TO_POINTER(id));
        if (node) {
            wire_plumber_service_fill_node(node, WP_GLOBAL_PROXY(obj), self);
        } else {
            node = wire_plumber_service_node_new(WP_GLOBAL_PROXY(obj), self);

            // connect to state changes to monitor devices state.
            g_signal_connect(WP_NODE(obj), "state-changed",
                             G_CALLBACK(on_state_change), self);

            g_hash_table_insert(self->db, GUINT_TO_POINTER(id), node);
            g_ptr_array_add(node_array, node);
        }

        if (node->id == default_node_id) *default_node = node;
    }
}

static void on_object_manager_change_get_links(WpObjectManager *om,
                                               WirePlumberService *self) {
    g_auto(GValue) value = G_VALUE_INIT;
    WpIterator *it = NULL;

    g_debug(
        "wireplumber_service.c:on_object_manager_change_get_links() called");

    it = wp_object_manager_new_filtered_iterator(self->om, WP_TYPE_LINK, NULL);

    for (; wp_iterator_next(it, &value); g_value_unset(&value)) {
        GObject *obj = g_value_get_object(&value);
        guint32 id = wp_proxy_get_bound_id(WP_PROXY(obj));
        WirePlumberServiceLink *link = NULL;

        link = g_hash_table_lookup(self->db, GUINT_TO_POINTER(id));
        if (link) {
            wire_plumber_service_fill_link(link, WP_GLOBAL_PROXY(obj), self);
        } else {
            link = wire_plumber_service_link_new(WP_GLOBAL_PROXY(obj), self);
            g_hash_table_insert(self->db, GUINT_TO_POINTER(id), link);
            g_ptr_array_add(self->links, link);
        }
    }
}

static void on_object_manager_change_get_audio_streams(
    WpObjectManager *om, WirePlumberService *self) {
    g_auto(GValue) value = G_VALUE_INIT;
    WpIterator *it = NULL;

    g_debug(
        "wireplumber_service.c:on_object_manager_change_get_audio_streams() "
        "called");

    it = wp_object_manager_new_filtered_iterator(
        self->om, WP_TYPE_NODE, WP_CONSTRAINT_TYPE_PW_PROPERTY,
        PW_KEY_MEDIA_CLASS, "#s", "Stream/*", NULL);

    for (; wp_iterator_next(it, &value); g_value_unset(&value)) {
        GObject *obj = g_value_get_object(&value);
        guint32 id = wp_proxy_get_bound_id(WP_PROXY(obj));
        WirePlumberServiceAudioStream *node = NULL;

        node = g_hash_table_lookup(self->db, GUINT_TO_POINTER(id));
        if (node) {
            wire_plumber_service_fill_audio_stream(node, WP_GLOBAL_PROXY(obj),
                                                   self);
        } else {
            node = wire_plumber_service_audio_stream_new(WP_GLOBAL_PROXY(obj),
                                                         self);

            // connect to state changes to monitor devices state.
            g_signal_connect(WP_NODE(obj), "state-changed",
                             G_CALLBACK(on_state_change), self);

            g_hash_table_insert(self->db, GUINT_TO_POINTER(id), node);
            g_ptr_array_add(self->streams, node);
        }
    }
}

static void on_object_manager_change(WpObjectManager *om,
                                     WirePlumberService *self) {
    g_auto(GValue) value = G_VALUE_INIT;

    g_debug("wireplumber_service.c:on_object_manager_change() called");

    // fill in default sink and source ids.
    g_signal_emit_by_name(self->default_nodes_api, "get-default-node",
                          "Audio/Sink", &self->default_sink_id);

    g_signal_emit_by_name(self->default_nodes_api, "get-default-node",
                          "Audio/Source", &self->default_source_id);

    // prune the database first
    wire_plumber_service_prune_db(self);

    // find sinks
    on_object_manager_change_get_source_sinks(om, self, "Audio/Sink");

    // find sources
    on_object_manager_change_get_source_sinks(om, self, "Audio/Source");

    // find audio streams
    on_object_manager_change_get_audio_streams(om, self);

    // find links
    on_object_manager_change_get_links(om, self);

    // emit database-changed signal
    g_signal_emit(self, service_signals[database_changed], 0, self->db);

    // debug default nodes ids
    g_debug(
        "wireplumber_service.c:on_object_manager_change() default sink id: %d "
        "default source id: %d",
        self->default_sink_id, self->default_source_id);
}

static void on_installed(WirePlumberService *self) {
    g_info(
        "wireplumber_service.c:wire_plumber_service_init() WirePlumberService "
        "initialized "
        "Name: %s Version: %s ",
        wp_core_get_remote_name(self->core),
        wp_core_get_remote_version(self->core));

    on_object_manager_change(self->om, self);

    // emit initial signals
    g_signal_emit(self, service_signals[default_sink_changed], 0,
                  self->default_sink);
    g_signal_emit(self, service_signals[default_source_changed], 0,
                  self->default_source);
    g_signal_emit(self, service_signals[node_changed], 0, self->db);
    g_signal_emit(self, service_signals[microphone_active], 0,
                  wire_plumber_service_microphone_active(self));

    // listen for object being added or removed from the pipewire server.
    g_signal_connect(self->om, "objects-changed",
                     G_CALLBACK(on_object_manager_change), self);

    // listen for audio events (volume, mute, etc...) from WirePlumber's mixer
    // api.
    g_signal_connect(self->mixer_api, "changed", G_CALLBACK(on_mixer_changed),
                     self);
};

static void on_plugin_activate(WpObject *p, GAsyncResult *res,
                               WirePlumberService *self) {
    GError *error = NULL;

    g_debug("wireplumber_service.c:on_plugin_activate() called");

    if (!wp_object_activate_finish(p, res, &error)) {
        g_error(
            "wireplumber_service.c:on_plugin_activate() failed to activate "
            "plugin: %s",
            error->message);
        return;
    }

    if (--self->pending_plugins == 0)
        wp_core_install_object_manager(self->core, self->om);
}

void context_state_cb(pa_context *c, void *self) {
    g_debug("wireplumber_service.c:context_state_cb() called");

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_READY: {
            g_debug(
                "wireplumber_service.c:context_state_cb() pulseaudio context "
                "ready");
            break;
        }
        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            g_critical(
                "wireplumber_service.c:context_state_cb() pulseaudio context "
                "failed");
            break;

        default:
            break;
    }
}

static void wire_plumber_service_init_pulseaudio(WirePlumberService *self) {
    g_debug(
        "wireplumber_service.c:wire_plumber_service_init_pulseaudio() called");

    self->pa_loop = pa_glib_mainloop_new(g_main_context_default());
    self->pa_api = pa_glib_mainloop_get_api(self->pa_loop);
    self->pa_ctx = pa_context_new(self->pa_api, "org.ldelossa.way-shell");

    pa_context_set_state_callback(self->pa_ctx, context_state_cb, self);

    if (pa_context_connect(self->pa_ctx, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) {
        g_error(
            "wireplumber_service.c:wire_plumber_service_init_pulseaudio() "
            "failed to connect to pulseaudio");
    }
}

static gboolean wire_plumber_service_connect_retry(gpointer user_data);

static void on_core_disconnect(WpCore *core, WirePlumberService *self) {
    g_debug("wireplumber_service.c:on_core_disconnect() called");
    g_critical(
        "wireplumber_service.c:on_core_disconnect() PipeWire connection lost");
    g_timeout_add_seconds(5, wire_plumber_service_connect_retry, self);
}

void on_default_nodes_api(GObject *source_object, GAsyncResult *res,
                          gpointer data) {
    // check res and if failure g_error

    GError *error = NULL;
    if (!wp_core_load_component_finish(WP_CORE(source_object), res, &error)) {
        g_error(
            "wireplumber_service.c:on_default_nodes_api() failed to load "
            "default-nodes-api plugin: %s",
            error->message);
    }

    WirePlumberService *self = (WirePlumberService *)data;
    // get the default nodes api plugin
    self->default_nodes_api = wp_plugin_find(self->core, "default-nodes-api");
    if (self->default_nodes_api == NULL) {
        g_error(
            "wireplumber_service.c:wire_plumber_service_init() failed to find "
            "default-nodes-api plugin");
    }

    // activate plugins
    wp_object_activate(WP_OBJECT(self->default_nodes_api),
                       WP_PLUGIN_FEATURE_ENABLED, NULL,
                       (GAsyncReadyCallback)on_plugin_activate, self);
}

void on_mixer_api_nodes_api(GObject *source_object, GAsyncResult *res,
                            gpointer data) {
    // check res and if failure g_error
    GError *error = NULL;
    if (!wp_core_load_component_finish(WP_CORE(source_object), res, &error)) {
        g_error(
            "wireplumber_service.c:on_mixer_api_nodes_api() failed to load "
            "mixer-api plugin: %s",
            error->message);
    }

    WirePlumberService *self = (WirePlumberService *)data;
    // get the mixer api plugin
    self->mixer_api = wp_plugin_find(self->core, "mixer-api");
    if (self->mixer_api == NULL) {
        g_error(
            "wireplumber_service.c:wire_plumber_service_init() failed to find "
            "mixer-api plugin");
    }

    wp_object_activate(WP_OBJECT(self->mixer_api), WP_PLUGIN_FEATURE_ENABLED,
                       NULL, (GAsyncReadyCallback)on_plugin_activate, self);
}

gboolean wire_plumber_service_connect(WirePlumberService *self) {
    g_debug("wireplumber_service.c:wire_plumber_service_connect() called");

    wp_init(WP_INIT_PIPEWIRE);

    // cleanup all existing arrays, hashtables, core and om
    if (self->db) g_hash_table_destroy(self->db);
    if (self->sinks) g_ptr_array_free(self->sinks, true);
    if (self->sources) g_ptr_array_free(self->sources, true);
    if (self->streams) g_ptr_array_free(self->streams, true);
    if (self->links) g_ptr_array_free(self->links, true);
    if (self->core) g_object_unref(self->core);
    if (self->om) g_object_unref(self->om);

    self->db = g_hash_table_new(g_direct_hash, g_direct_equal);
    self->sources = g_ptr_array_new();
    self->sinks = g_ptr_array_new();
    self->streams = g_ptr_array_new();
    self->links = g_ptr_array_new();

    self->core = wp_core_new(NULL, NULL, NULL);
    self->om = wp_object_manager_new();
    self->pending_plugins = 2;

    WpObjectInterest *all_nodes = wp_object_interest_new_type(WP_TYPE_NODE);
    // enabling these features will determine what 'events' are set when the
    // WpObject params change. We want to know when the SPA data has changed
    // so we need both _ROUTE and _PROPS feaures enabled.
    wp_object_manager_request_object_features(
        self->om, WP_TYPE_NODE,
        WP_PROXY_FEATURE_BOUND | WP_PIPEWIRE_OBJECT_FEATURE_PARAM_PROPS |
            WP_PIPEWIRE_OBJECT_FEATURE_INFO |
            WP_PIPEWIRE_OBJECT_FEATURE_PARAM_ROUTE | WP_NODE_FEATURE_PORTS);

    WpObjectInterest *all_links = wp_object_interest_new_type(WP_TYPE_LINK);
    wp_object_manager_request_object_features(self->om, WP_TYPE_LINK,
                                              WP_PROXY_FEATURE_BOUND);

    wp_object_manager_add_interest_full(self->om, all_nodes);
    wp_object_manager_add_interest_full(self->om, all_links);

    // load the mixer and default nodes apis.
    wp_core_load_component(self->core,
                           "libwireplumber-module-default-nodes-api", "module",
                           NULL, NULL, NULL, on_default_nodes_api, self);

    wp_core_load_component(self->core, "libwireplumber-module-mixer-api",
                           "module", NULL, NULL, NULL, on_mixer_api_nodes_api,
                           self);

    // make a connection to PipeWire
    if (!wp_core_connect(self->core)) {
        g_critical(
            "wireplumber_service.c:wire_plumber_service_init() failed to "
            "connect to PipeWire daemon");
        return false;
    }

    // signal which runs after object manager is installed on core (after plugin
    // activation)
    g_signal_connect_swapped(self->om, "installed", G_CALLBACK(on_installed),
                             self);

    wire_plumber_service_init_pulseaudio(self);

    // attach to core's disconnect signal
    g_signal_connect(self->core, "disconnected", G_CALLBACK(on_core_disconnect),
                     self);

    return true;
}

static gboolean wire_plumber_service_connect_retry(gpointer user_data) {
    g_debug(
        "wireplumber_service.c:wire_plumber_service_connect_retry() called");

    WirePlumberService *self = (WirePlumberService *)user_data;

    if (!wire_plumber_service_connect(self)) {
        g_debug(
            "wireplumber_service.c:wire_plumber_service_connect_retry() failed "
            "to connect to PipeWire, retrying in 5 seconds");
        return G_SOURCE_CONTINUE;
    }

    return G_SOURCE_REMOVE;
}

static void wire_plumber_service_init(WirePlumberService *self) {
    g_debug("wireplumber_service.c:wire_plumber_service_init() called");

    if (!wire_plumber_service_connect(self)) {
        g_timeout_add_seconds(5, wire_plumber_service_connect_retry, self);
    }
};

int wire_plumber_service_global_init() {
    g_debug("wireplumber_service.c:wire_plumber_service_global_init() called");
    if (global == NULL) {
        global = g_object_new(WIRE_PLUMBER_SERVICE_TYPE, NULL);
    }
    return 0;
}

WirePlumberService *wire_plumber_service_get_global() { return global; }

WirePlumberServiceNode *wire_plumber_service_get_default_sink(
    WirePlumberService *self) {
    g_debug(
        "wireplumber_service.c:wire_plumber_service_get_default_sink() "
        "called");
    return self->default_sink;
}

WirePlumberServiceNode *wire_plumber_service_get_default_source(
    WirePlumberService *self) {
    g_debug(
        "wireplumber_service.c:wire_plumber_service_get_default_source() "
        "called");
    return self->default_source;
}

GPtrArray *wire_plumber_service_get_sinks(WirePlumberService *self) {
    g_debug("wireplumber_service.c:wire_plumber_service_get_sinks() called");
    return self->sinks;
}

GPtrArray *wire_plumber_service_get_sources(WirePlumberService *self) {
    g_debug("wireplumber_service.c:wire_plumber_service_get_sources() called");
    return self->sources;
}

GPtrArray *wire_plumber_service_get_streams(WirePlumberService *self) {
    g_debug("wireplumber_service.c:wire_plumber_service_get_streams() called");
    return self->streams;
}

GPtrArray *wire_plumber_service_get_links(WirePlumberService *self) {
    g_debug("wireplumber_service.c:wire_plumber_service_get_streams() called");
    return self->links;
}

void sink_input_info_cb(pa_context *c, const pa_sink_input_info *i, int eol,
                        void *userdata) {
    if (eol > 0) {
        return;
    }

    WirePlumberService *self = (WirePlumberService *)userdata;

    // we want to find the sink-input that matches our pending output stream.
    const char *key = "object.id";
    const char *value;

    value = pa_proplist_gets(i->proplist, key);
    if (!value) return;

    guint32 id = g_ascii_strtoull(value, NULL, 10);
    if (id != self->pending_output_stream) return;

    g_debug(
        "wireplumber_service.c:sink_input_info_cb() found sink-input id: %d, "
        "for wireplumber sink id: %d",
        i->index, id);

    // we found the sink-input, now get our WirePlumberServiceNode for the sink
    // we are linking to.
    WirePlumberServiceNode *node =
        g_hash_table_lookup(self->db, GUINT_TO_POINTER(self->pending_sink));

    if (!node) return;

    // perform move-sink-input pa operation
    pa_operation *o = pa_context_move_sink_input_by_name(
        self->pa_ctx, i->index, node->proper_name, NULL, NULL);
    if (o) {
        pa_operation_unref(o);
    }
}

void source_output_info_cb(pa_context *c, const pa_source_output_info *i,
                           int eol, void *userdata) {
    if (eol > 0) {
        return;
    }

    WirePlumberService *self = (WirePlumberService *)userdata;

    const char *key = "object.id";
    const char *value;

    value = pa_proplist_gets(i->proplist, key);
    if (!value) return;

    guint32 id = g_ascii_strtoull(value, NULL, 10);
    if (id != self->pending_input_stream) return;

    g_debug(
        "wireplumber_service.c:source_output_info_cb() found source-output id: "
        "%d, "
        "for wireplumber sink id: %d",
        i->index, id);

    // we found the output source, now find the WirePlumberServiceNode for the
    // source.
    WirePlumberServiceNode *node =
        g_hash_table_lookup(self->db, GUINT_TO_POINTER(self->pending_source));

    if (!node) return;

    // perform move-sink-input pa operation
    pa_operation *o = pa_context_move_source_output_by_name(
        self->pa_ctx, i->index, node->proper_name, NULL, NULL);
    if (o) {
        pa_operation_unref(o);
    }
}

void wire_plumber_service_set_link(WirePlumberService *self,
                                   WirePlumberServiceNodeHeader *output,
                                   WirePlumberServiceNodeHeader *input) {
    g_debug("wireplumber_service.c:wire_plumber_service_set_link() called");

    // TODO: this should really be data tied to a 'request-id', such that
    // multiple link changes can occur at once, creating a new 'request' each
    // time.
    self->pending_input_stream = 0;
    self->pending_output_stream = 0;
    self->pending_sink = 0;
    self->pending_source = 0;

    // determine which one is our stream
    WirePlumberServiceNode *node;
    WirePlumberServiceAudioStream *stream;

    switch (output->type) {
        case WIRE_PLUMBER_SERVICE_TYPE_SINK:
        case WIRE_PLUMBER_SERVICE_TYPE_SOURCE:
            node = (WirePlumberServiceNode *)output;
            break;
        case WIRE_PLUMBER_SERVICE_TYPE_INPUT_AUDIO_STREAM:
        case WIRE_PLUMBER_SERVICE_TYPE_OUTPUT_AUDIO_STREAM:
            stream = (WirePlumberServiceAudioStream *)output;
            break;
        default:
            break;
    }

    switch (input->type) {
        case WIRE_PLUMBER_SERVICE_TYPE_SINK:
        case WIRE_PLUMBER_SERVICE_TYPE_SOURCE:
            node = (WirePlumberServiceNode *)input;
            break;
        case WIRE_PLUMBER_SERVICE_TYPE_INPUT_AUDIO_STREAM:
        case WIRE_PLUMBER_SERVICE_TYPE_OUTPUT_AUDIO_STREAM:
            stream = (WirePlumberServiceAudioStream *)input;
            break;
        default:
            break;
    }

    // we need to now query the pulse audio api to find the 'sink-input' or
    // 'source-outputs' which refer to our stream and move it to our discovered
    // node.
    if (stream->type == WIRE_PLUMBER_SERVICE_TYPE_INPUT_AUDIO_STREAM) {
        self->pending_input_stream = stream->id;
        self->pending_source = node->id;
        pa_operation *o = pa_context_get_source_output_info_list(
            self->pa_ctx, source_output_info_cb, self);
        if (o) {
            pa_operation_unref(o);
        }
    }
    if (stream->type == WIRE_PLUMBER_SERVICE_TYPE_OUTPUT_AUDIO_STREAM) {
        self->pending_output_stream = stream->id;
        self->pending_sink = node->id;
        pa_operation *o = pa_context_get_sink_input_info_list(
            self->pa_ctx, sink_input_info_cb, self);
        if (o) {
            pa_operation_unref(o);
        }
    }
}

GHashTable *wire_plumber_service_get_db(WirePlumberService *self) {
    g_debug("wireplumber_service.c:wire_plumber_service_get_db() called");
    return self->db;
}

void wire_plumber_service_set_volume(WirePlumberService *self,
                                     const WirePlumberServiceNode *node,
                                     double volume) {
    g_debug(
        "wireplumber_service.c:wire_plumber_service_set_volume() called: %f",
        volume);

    if (!node) return;

    g_auto(GVariantBuilder) b = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
    GVariant *variant = NULL;
    gboolean res = FALSE;

    // our volume is in a linear scale for ease of use, but mixer-api wants it
    // in a cubic scale, to take the cube of our linear volume before encoding
    g_variant_builder_add(
        &b, "{sv}", "volume",
        g_variant_new_double(volume_to_linear(volume, SCALE_CUBIC)));
    variant = g_variant_builder_end(&b);

    g_signal_emit_by_name(self->mixer_api, "set-volume", node->id, variant,
                          &res);
    g_debug(
        "wireplumber_service.c:wire_plumber_service_set_volume() id: %d, "
        "res: %d",
        node->id, res);
}

void wire_plumber_service_volume_up(WirePlumberService *self,
                                    const WirePlumberServiceNode *node) {
    g_debug("wireplumber_service.c:wire_plumber_service_volume_up() called");

    if (!node) return;

    if (node->volume >= 1.0) {
        g_debug(
            "wireplumber_service.c:wire_plumber_service_volume_up() volume is "
            "already at max");
        return;
    }
    double volume = node->volume + .05;
    g_debug("wireplumber_service.c:wire_plumber_service_volume_up() volume: %f",
            volume);
    wire_plumber_service_set_volume(self, node, volume);
}

void wire_plumber_service_volume_down(WirePlumberService *self,
                                      const WirePlumberServiceNode *node) {
    g_debug("wireplumber_service.c:wire_plumber_service_volume_down() called");

    if (!node) return;

    if (node->volume <= 0.0) {
        g_debug(
            "wireplumber_service.c:wire_plumber_service_volume_down() volume "
            "is already at min");
        return;
    }
    double volume = node->volume - .05;
    g_debug(
        "wireplumber_service.c:wire_plumber_service_volume_down() volume: %f",
        volume);
    wire_plumber_service_set_volume(self, node, volume);
}

void wire_plumber_service_volume_mute(WirePlumberService *self,
                                      WirePlumberServiceNode *node) {
    gboolean res = FALSE;

    if (!node) return;

    // record the volume before mute, since mute sets node's volume to 0
    node->last_volume = node->volume;

    g_debug("wireplumber_service.c:wire_plumber_service_volume_mute() called");

    g_auto(GVariantBuilder) b = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
    GVariant *variant = NULL;

    g_variant_builder_add(&b, "{sv}", "mute", g_variant_new_boolean(true));
    variant = g_variant_builder_end(&b);

    g_signal_emit_by_name(self->mixer_api, "set-volume", node->id, variant,
                          &res);

    g_debug(
        "wireplumber_service.c:wire_plumber_service_volume_mute() id: %d, res: "
        "%d",
        node->id, res);
}

void wire_plumber_service_volume_unmute(WirePlumberService *self,
                                        const WirePlumberServiceNode *node) {
    g_debug(
        "wireplumber_service.c:wire_plumber_service_volume_unmute() called");

    if (!node) return;

    gboolean res = FALSE;

    g_debug("wireplumber_service.c:wire_plumber_service_volume_mute() called");

    g_auto(GVariantBuilder) b = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
    GVariant *variant = NULL;

    g_variant_builder_add(&b, "{sv}", "mute", g_variant_new_boolean(false));
    variant = g_variant_builder_end(&b);

    g_signal_emit_by_name(self->mixer_api, "set-volume", node->id, variant,
                          &res);

    // restore the recorded volume before mute occurred.
    wire_plumber_service_set_volume(self, node, node->last_volume);

    g_debug(
        "wireplumber_service.c:wire_plumber_service_volume_mute() id: %d, res: "
        "%d",
        node->id, res);
}

char *wire_plumber_service_map_source_vol_icon(float vol, gboolean mute) {
    if (mute) {
        return "microphone-sensitivity-muted-symbolic";
    }
    if (vol < 0.25) {
        return "microphone-sensitivity-low-symbolic";
    }
    if (vol >= 0.25 && vol < 0.5) {
        return "microphone-sensitivity-medium-symbolic";
    }
    return "microphone-sensitivity-high-symbolic";
}

char *wire_plumber_service_map_sink_vol_icon(float vol, gboolean mute) {
    if (mute) {
        return "audio-volume-muted-symbolic";
    }
    if (vol < 0.25) {
        return "audio-volume-low-symbolic";
    }
    if (vol >= 0.25 && vol < 0.5) {
        return "audio-volume-medium-symbolic";
    }
    return "audio-volume-high-symbolic";
}

enum WirePlumberServiceType wire_plumber_service_media_class_to_type(
    const char *media_class) {
    if (g_strcmp0(media_class, "Audio/Sink") == 0) {
        return WIRE_PLUMBER_SERVICE_TYPE_SINK;
    }
    if (g_strcmp0(media_class, "Audio/Source") == 0) {
        return WIRE_PLUMBER_SERVICE_TYPE_SOURCE;
    }
    if (g_strcmp0(media_class, "Stream/Input/Audio") == 0) {
        return WIRE_PLUMBER_SERVICE_TYPE_INPUT_AUDIO_STREAM;
    }
    if (g_strcmp0(media_class, "Stream/Output/Audio") == 0) {
        return WIRE_PLUMBER_SERVICE_TYPE_OUTPUT_AUDIO_STREAM;
    }
    return WIRE_PLUMBER_SERVICE_TYPE_UNKNOWN;
}
