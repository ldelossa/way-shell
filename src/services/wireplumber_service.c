#include "wireplumber_service.h"

#include <adwaita.h>
#include <pipewire/keys.h>
#include <wireplumber-0.4/wp/wp.h>

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
    // the default sink has changed, a signal with the default source is
    // emitted.
    default_source_changed,
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
    GPtrArray *sinks;
    GPtrArray *sources;
    GPtrArray *streams;
    GPtrArray *links;
    GHashTable *db;
    gint32 pending_link_removal;
    WirePlumberServiceNodeHeader *pending_link_output;
    WirePlumberServiceNodeHeader *pending_link_input;
    int pending_plugins;
};

static guint service_signals[signals_n] = {0};
G_DEFINE_TYPE(WirePlumberService, wire_plumber_service, G_TYPE_OBJECT);

// stub out dispose, finalize, class init and init methods for this GObject
// class.
static void wire_plumber_service_dispose(GObject *gobject) {
    WirePlumberService *self = WIRE_PLUMBER_SERVICE(gobject);

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

    // define 'default_source_changed' signal
    service_signals[default_source_changed] =
        g_signal_new("default-source-changed", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
                     G_TYPE_POINTER);

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

static void wire_plumber_service_fill_ports(
    WpNode *node, WirePlumberServiceNodeHeader *header,
    WirePlumberService *self) {
    g_debug("wireplumber_service.c:wire_plumber_service_fill_ports() called");

    GValue value = G_VALUE_INIT;

    WpIterator *i = wp_node_new_ports_iterator(node);
    for (; wp_iterator_next(i, &value); g_value_unset(&value)) {
        WpPort *port = g_value_get_object(&value);

        const char *direction = wp_pipewire_object_get_property(
            WP_PIPEWIRE_OBJECT(port), PW_KEY_PORT_DIRECTION);
        const char *monitor = wp_pipewire_object_get_property(
            WP_PIPEWIRE_OBJECT(port), PW_KEY_PORT_MONITOR);
        const char *channel = wp_pipewire_object_get_property(
            WP_PIPEWIRE_OBJECT(port), PW_KEY_AUDIO_CHANNEL);

        // if its a monitor port, just ignore it
        if (monitor) continue;

        // eliminate ports in directions we don't care about
        switch (header->type) {
            case WIRE_PLUMBER_SERVICE_TYPE_SINK:
                if (g_strcmp0(direction, "in") != 0) continue;
                break;
            case WIRE_PLUMBER_SERVICE_TYPE_SOURCE:
                if (g_strcmp0(direction, "out") != 0) continue;
                break;
            case WIRE_PLUMBER_SERVICE_TYPE_INPUT_AUDIO_STREAM:
                if (g_strcmp0(direction, "in") != 0) continue;
                break;
            case WIRE_PLUMBER_SERVICE_TYPE_OUTPUT_AUDIO_STREAM:
                if (g_strcmp0(direction, "out") != 0) continue;
                break;
            default:
                break;
        }

        guint32 id = wp_proxy_get_bound_id(WP_PROXY(port));

        // map to wireplumber service port
        enum WirePlumberServicePortChannel port_channel =
            wire_plumber_service_map_port(channel);

        g_debug(
            "wireplumber_service.c:wire_plumber_service_fill_ports() id: %d, "
            "direction: %s, channel: %s, port_channel: %d",
            id, direction, channel, port_channel);

        header->ports[port_channel] = id;
    }
}

static void wire_plumber_service_fill_audio_stream(
    WirePlumberServiceAudioStream *node, WpGlobalProxy *proxy,
    WirePlumberService *self) {
    WpProperties *props = wp_global_proxy_get_global_properties(proxy);
    GVariant *mixer_values = NULL;
    GValue value = G_VALUE_INIT;

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

    // convert volume to cubic
    node->volume = volume_from_linear(node->volume, SCALE_CUBIC);

    // fill in WpNode info.
    WpIterator *i = wp_properties_new_iterator(props);
    for (; wp_iterator_next(i, &value); g_value_unset(&value)) {
        WpPropertiesItem *item = g_value_get_boxed(&value);

        const gchar *key = wp_properties_item_get_key(item);
        const gchar *val = wp_properties_item_get_value(item);

        if (g_strcmp0(key, PW_KEY_NODE_NAME) == 0) {
            node->name = g_strdup(val);
        }
        if (g_strcmp0(key, PW_KEY_APP_NAME) == 0) {
            node->app_name = g_strdup(val);
        }
        if (g_strcmp0(key, PW_KEY_MEDIA_CLASS) == 0) {
            node->media_class = g_strdup(val);
        }
    }

    // check media type and set actual type
    if (g_strcmp0(node->media_class, "Stream/Output/Audio") == 0) {
        node->type = WIRE_PLUMBER_SERVICE_TYPE_OUTPUT_AUDIO_STREAM;
    }
    if (g_strcmp0(node->media_class, "Stream/Input/Audio") == 0) {
        node->type = WIRE_PLUMBER_SERVICE_TYPE_INPUT_AUDIO_STREAM;
    }

    // fill ports
    wire_plumber_service_fill_ports(WP_NODE(proxy),
                                    (WirePlumberServiceNodeHeader *)node, self);
}

static void wire_plumber_service_fill_node(WirePlumberServiceNode *node,
                                           WpGlobalProxy *proxy,
                                           WirePlumberService *self) {
    WpProperties *props = wp_global_proxy_get_global_properties(proxy);
    GVariant *mixer_values = NULL;
    GValue value = G_VALUE_INIT;

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

    // convert volume to cubic
    node->volume = volume_from_linear(node->volume, SCALE_CUBIC);

    // fill in WpNode info.
    WpIterator *i = wp_properties_new_iterator(props);
    for (; wp_iterator_next(i, &value); g_value_unset(&value)) {
        WpPropertiesItem *item = g_value_get_boxed(&value);

        const gchar *key = wp_properties_item_get_key(item);
        const gchar *val = wp_properties_item_get_value(item);

        if (g_strcmp0(key, PW_KEY_MEDIA_CLASS) == 0) {
            node->media_class = g_strdup(val);
        }
        if (g_strcmp0(key, PW_KEY_NODE_DESCRIPTION) == 0) {
            node->name = g_strdup(val);
        }
        if (g_strcmp0(key, PW_KEY_NODE_NICK) == 0) {
            node->nick_name = g_strdup(val);
        }
    }

    // check media type and set actual type
    if (g_strcmp0(node->media_class, "Audio/Sink") == 0) {
        node->type = WIRE_PLUMBER_SERVICE_TYPE_SINK;
    }
    if (g_strcmp0(node->media_class, "Audio/Source") == 0) {
        node->type = WIRE_PLUMBER_SERVICE_TYPE_SOURCE;
    }

    // fill ports
    wire_plumber_service_fill_ports(WP_NODE(proxy),
                                    (WirePlumberServiceNodeHeader *)node, self);
}

static void wire_plumber_service_fill_link(WirePlumberServiceLink *link,
                                           WpGlobalProxy *proxy,
                                           WirePlumberService *self) {
    GValue value = G_VALUE_INIT;
    WpProperties *props = wp_global_proxy_get_global_properties(proxy);

    link->id = wp_proxy_get_bound_id(WP_PROXY(proxy));

    // fill in Link info
    WpIterator *i = wp_properties_new_iterator(props);
    for (; wp_iterator_next(i, &value); g_value_unset(&value)) {
        WpPropertiesItem *item = g_value_get_boxed(&value);

        const gchar *key = wp_properties_item_get_key(item);
        const gchar *val = wp_properties_item_get_value(item);

        if (g_strcmp0(key, PW_KEY_LINK_INPUT_NODE) == 0) {
            link->input_node = g_ascii_strtoull(val, NULL, 10);
        }
        if (g_strcmp0(key, PW_KEY_LINK_OUTPUT_NODE) == 0) {
            link->output_node = g_ascii_strtoull(val, NULL, 10);
        }
    }
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
        }

        if (node == self->default_source) {
            g_signal_emit(self, service_signals[default_source_changed], 0,
                          node);
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

    // iterate and create sinks
    it = wp_object_manager_new_iterator(self->om);

    for (; wp_iterator_next(it, &value); g_value_unset(&value)) {
        GObject *obj = g_value_get_object(&value);
        guint32 id = wp_proxy_get_bound_id(WP_PROXY(obj));
        g_hash_table_add(set, GUINT_TO_POINTER(id));
    }

    // prune db hashmap
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
                g_free(node);
            }
        }

    // prune source array
    if (self->sinks->len > 0)
        for (int i = self->sources->len - 1; i >= 0; i--) {
            WirePlumberServiceNode *node = g_ptr_array_index(self->sources, i);
            if (!g_hash_table_contains(set, GUINT_TO_POINTER(node->id))) {
                g_ptr_array_remove_index(self->sources, i);
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
    enum WirePlumberServiceType type = WIRE_PLUMBER_SERVICE_TYPE_UNKNOWN;
    guint32 default_node_id = 0;

    g_debug("wireplumber_service.c:on_object_manager_change() called");

    if (g_strcmp0(media_class, "Audio/Sink") == 0) {
        type = WIRE_PLUMBER_SERVICE_TYPE_SINK;
        node_array = self->sinks;
        default_node = &self->default_sink;
        default_node_id = self->default_sink_id;
    }

    if (g_strcmp0(media_class, "Audio/Source") == 0) {
        type = WIRE_PLUMBER_SERVICE_TYPE_SOURCE;
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

    // find output audio streams
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

    // listne for object manager changes
    g_signal_connect(self->om, "objects-changed",
                     G_CALLBACK(on_object_manager_change), self);

    // listen for mixer changes
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

static void wire_plumber_service_init(WirePlumberService *self) {
    GError *error = NULL;

    g_debug("wireplumber_service.c:wire_plumber_service_init() called");
    wp_init(WP_INIT_PIPEWIRE);

    self->db = g_hash_table_new(g_direct_hash, g_direct_equal);
    self->sources = g_ptr_array_new();
    self->sinks = g_ptr_array_new();
    self->streams = g_ptr_array_new();
    self->links = g_ptr_array_new();

    self->core = wp_core_new(NULL, NULL);
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

    WpObjectInterest *all_ports = wp_object_interest_new_type(WP_TYPE_PORT);
    wp_object_manager_request_object_features(self->om, WP_TYPE_PORT,
                                              WP_PROXY_FEATURE_BOUND);

    WpObjectInterest *all_links = wp_object_interest_new_type(WP_TYPE_LINK);
    wp_object_manager_request_object_features(
        self->om, WP_TYPE_LINK,
        WP_PROXY_FEATURE_BOUND | WP_PIPEWIRE_OBJECT_FEATURE_PARAM_PROPS |
            WP_PIPEWIRE_OBJECT_FEATURE_INFO |
            WP_PIPEWIRE_OBJECT_FEATURE_PARAM_ROUTE |
            WP_LINK_FEATURE_ESTABLISHED);

    wp_object_manager_add_interest_full(self->om, all_nodes);
    wp_object_manager_add_interest_full(self->om, all_ports);
    wp_object_manager_add_interest_full(self->om, all_links);

    // load the mixer and default nodes apis.
    if (!wp_core_load_component(self->core,
                                "libwireplumber-module-default-nodes-api",
                                "module", NULL, &error)) {
        g_error(
            "wireplumber_service.c:wire_plumber_service_init() failed to load "
            "default-nodes-api module: %s",
            error->message);
    }
    if (!wp_core_load_component(self->core, "libwireplumber-module-mixer-api",
                                "module", NULL, &error)) {
        g_error(
            "wireplumber_service.c:wire_plumber_service_init() failed to load "
            "mixer-api module: %s",
            error->message);
    }

    // make a connection to PipeWire
    if (!wp_core_connect(self->core))
        g_error(
            "wireplumber_service.c:wire_plumber_service_init() failed to "
            "connect to PipeWire daemon");

    // get the default nodes api plugin
    self->default_nodes_api = wp_plugin_find(self->core, "default-nodes-api");
    if (self->default_nodes_api == NULL) {
        g_error(
            "wireplumber_service.c:wire_plumber_service_init() failed to find "
            "default-nodes-api plugin");
    }

    // get the mixer api plugin
    self->mixer_api = wp_plugin_find(self->core, "mixer-api");
    if (self->mixer_api == NULL) {
        g_error(
            "wireplumber_service.c:wire_plumber_service_init() failed to find "
            "mixer-api plugin");
    }

    // signal which runs after object manager is installed on core (after plugin
    // activation)
    g_signal_connect_swapped(self->om, "installed", G_CALLBACK(on_installed),
                             self);

    // activate plugins
    wp_object_activate(WP_OBJECT(self->default_nodes_api),
                       WP_PLUGIN_FEATURE_ENABLED, NULL,
                       (GAsyncReadyCallback)on_plugin_activate, self);

    wp_object_activate(WP_OBJECT(self->mixer_api), WP_PLUGIN_FEATURE_ENABLED,
                       NULL, (GAsyncReadyCallback)on_plugin_activate, self);
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

static void activate_error_cb(WpObject *object, GAsyncResult *res,
                              WirePlumberService *self) {
    GError *error = NULL;
    g_debug("wireplumber_service.c:activate_error_cb() called");
    wp_object_activate_finish(object, res, &error);
    if (error) {
        g_warning("wireplumber_service.c:activate_error_cb() error: %s",
                  error->message);
        g_error_free(error);
        return;
    }
}

static void on_link_removed(WpGlobalProxy *proxy, WirePlumberService *self) {
    g_debug("wireplumber_service.c:on_link_removed() called");

    self->pending_link_removal--;
    if (self->pending_link_removal > 0) {
        g_debug(
            "wireplumber_service.c:on_link_removed() pending_link_removal: %d",
            self->pending_link_removal);
        return;
    }
    g_debug("wireplumber_service.c:on_link_removed() pending_link_removal: %d",
            self->pending_link_removal);

    // if either of our pending inputs or outputs were nil, this indicates we
    // just wanted to unlink the non-null side, no need to create any links.
    if (!self->pending_link_input || !self->pending_link_output) return;

    GPtrArray *links_to_create = g_ptr_array_new();

    g_debug(
        "wireplumber_service.c:on_link_removed() pending_link_input: %d, "
        "pending_link_output: %d",
        self->pending_link_input->id, self->pending_link_output->id);

    // for every valid port on the output attach it to a cooresponding port
    // on the input.
    for (int i = WIRE_PLUMBER_SERVICE_PORT_RL;
         i <= WIRE_PLUMBER_SERVICE_PORT_TFR; i++) {
        if (self->pending_link_output->ports[i] == 0) continue;

        // input doesn't have this port.
        if (self->pending_link_input->ports[i] == 0) continue;

        const char *output_port =
            g_strdup_printf("%d", self->pending_link_output->ports[i]);
        const char *output_node =
            g_strdup_printf("%d", self->pending_link_output->id);
        const char *input_node =
            g_strdup_printf("%d", self->pending_link_input->id);
        const char *input_port =
            g_strdup_printf("%d", self->pending_link_input->ports[i]);

        g_debug(
            "wireplumber_service.c:on_link_removed() creating link "
            "channel: %d "
            "output_port: %s, "
            "output_node: %s, input_node: %s, input_port: %s",
            i, output_port, output_node, input_node, input_port);

        WpLink *link_new = wp_link_new_from_factory(
            self->core, "link-factory",
            wp_properties_new("link.output.node", output_node,
                              "link.output.port", output_port,
                              "link.input.node", input_node, "link.input.port",
                              input_port, NULL));

        g_ptr_array_add(links_to_create, link_new);
    }

    for (int i = 0; i < links_to_create->len; i++) {
        WpLink *wp_link = g_ptr_array_index(links_to_create, i);
        wp_object_activate(WP_OBJECT(wp_link), WP_PROXY_FEATURE_BOUND, NULL,
                           (GAsyncReadyCallback)activate_error_cb, self);
    }

    g_ptr_array_unref(links_to_create);
}

void wire_plumber_service_set_link(WirePlumberService *self,
                                   WirePlumberServiceNodeHeader *output,
                                   WirePlumberServiceNodeHeader *input) {
    g_debug("wireplumber_service.c:wire_plumber_service_set_link() called");

    self->pending_link_removal = 0;
    self->pending_link_input = input;
    self->pending_link_output = output;

    GPtrArray *links_to_remove = g_ptr_array_new();

    // print links length
    g_debug(
        "wireplumber_service.c:wire_plumber_service_set_link() links length: "
        "%d",
        self->links->len);

    // find all links which match our inputs and outputs and mark them for
    // removal
    for (int i = 0; i < self->links->len; i++) {
        WirePlumberServiceLink *link = g_ptr_array_index(self->links, i);

        g_debug(
            "wireplumber_service.c:wire_plumber_service_set_link() link: %d, "
            "input_node: %d, output_node: %d",
            link->id, link->input_node, link->output_node);

        WpLink *wp_link = wp_object_manager_lookup(
            self->om, WP_TYPE_LINK, WP_CONSTRAINT_TYPE_G_PROPERTY, "bound-id",
            "=u", link->id, NULL);
        if (!wp_link) {
            g_debug(
                "wireplumber_service.c:wire_plumber_service_set_link() link "
                "not found in object manager, aborting.");
            return;
        }

        if (link->output_node == output->id) {
            g_ptr_array_add(links_to_remove, wp_link);
            g_debug(
                "wireplumber_service.c:wire_plumber_service_set_link() "
                "pending removal of link: %d",
                link->id);
            self->pending_link_removal++;
        }

        if (link->input_node == input->id) {
            g_ptr_array_add(links_to_remove, wp_link);
            g_debug(
                "wireplumber_service.c:wire_plumber_service_set_link() "
                "pending removal of link: %d",
                link->id);
            self->pending_link_removal++;
        }
    }

    if (links_to_remove->len == 0) on_link_removed(NULL, self);

    for (int i = 0; i < links_to_remove->len; i++) {
        WpLink *wp_link = g_ptr_array_index(links_to_remove, i);
        g_signal_connect(WP_GLOBAL_PROXY(wp_link), "pw-proxy-destroyed",
                         G_CALLBACK(on_link_removed), self);
    }

    for (int i = 0; i < links_to_remove->len; i++) {
        WpLink *wp_link = g_ptr_array_index(links_to_remove, i);
        wp_global_proxy_request_destroy(WP_GLOBAL_PROXY(wp_link));
    }

    g_ptr_array_unref(links_to_remove);
}

GHashTable *wire_plumber_service_get_db(WirePlumberService *self) {
    g_debug("wireplumber_service.c:wire_plumber_service_get_db() called");
    return self->db;
}

void wire_plumber_service_set_volume(WirePlumberService *self,
                                     const WirePlumberServiceNode *node,
                                     float volume) {
    g_debug(
        "wireplumber_service.c:wire_plumber_service_set_volume() called: %f",
        volume);

    g_auto(GVariantBuilder) b = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
    GVariant *variant = NULL;
    gboolean res = FALSE;

    g_variant_builder_add(&b, "{sv}", "volume", g_variant_new_double(volume));
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
    if (node->volume == 1.0) {
        g_debug(
            "wireplumber_service.c:wire_plumber_service_volume_up() volume is "
            "already at max");
        return;
    }
    float volume = node->volume + .5;
    wire_plumber_service_set_volume(self, node, volume);
}

void wire_plumber_service_volume_down(WirePlumberService *self,
                                      const WirePlumberServiceNode *node) {
    g_debug("wireplumber_service.c:wire_plumber_service_volume_down() called");

    if (node->volume == 0.0) {
        g_debug(
            "wireplumber_service.c:wire_plumber_service_volume_down() volume "
            "is already at min");
        return;
    }
    float volume = node->volume - .5;
    wire_plumber_service_set_volume(self, node, volume);
}

void wire_plumber_service_volume_mute(WirePlumberService *self,
                                      const WirePlumberServiceNode *node) {
    gboolean res = FALSE;

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

    gboolean res = FALSE;

    g_debug("wireplumber_service.c:wire_plumber_service_volume_mute() called");

    g_auto(GVariantBuilder) b = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
    GVariant *variant = NULL;

    g_variant_builder_add(&b, "{sv}", "mute", g_variant_new_boolean(false));
    variant = g_variant_builder_end(&b);

    g_signal_emit_by_name(self->mixer_api, "set-volume", node->id, variant,
                          &res);

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
