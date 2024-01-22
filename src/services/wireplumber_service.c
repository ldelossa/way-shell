#include "wireplumber_service.h"

#include <adwaita.h>
#include <pipewire/keys.h>
#include <wireplumber-0.4/wp/wp.h>

#include "wp/core.h"
#include "wp/node.h"
#include "wp/proxy-interfaces.h"

enum signals { default_nodes_change, signals_n };

static WirePlumberService *global = NULL;

struct _WirePlumberService {
    GObject parent_instance;
    WpCore *core;
    WpObjectManager *obj_mgr;
    WpPlugin *default_nodes_api;
    WpPlugin *mixer_api;
    WirePlumberServiceDefaultNodes default_nodes;
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

    // define 'default_nodes_changed' signal
    service_signals[default_nodes_change] = g_signal_new(
        "default-nodes-changed", G_TYPE_FROM_CLASS(object_class),
        G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT);
};

static void set_default_sink(WirePlumberService *self) {
    GVariant *mixer = NULL;
    WpNode *sink = NULL;

    g_debug("wireplumber_service.c:set_default_sink() called");

    g_signal_emit_by_name(self->mixer_api, "get-volume",
                          self->default_nodes.sink.id, &mixer);

    if (!mixer)
        g_error("wireplumber_service.c:set_default_sink() failed to get mixer");

    g_variant_lookup(mixer, "volume", "d", &self->default_nodes.sink.volume);
    g_variant_lookup(mixer, "mute", "b", &self->default_nodes.sink.mute);
    g_variant_lookup(mixer, "step", "b", &self->default_nodes.sink.step);
    g_variant_lookup(mixer, "base", "d", &self->default_nodes.sink.base);

    // convert volume to cubic
    self->default_nodes.sink.volume =
        volume_from_linear(self->default_nodes.sink.volume, SCALE_CUBIC);

    sink = wp_object_manager_lookup(self->obj_mgr, WP_TYPE_NODE,
                                    WP_CONSTRAINT_TYPE_G_PROPERTY, "bound-id",
                                    "=u", self->default_nodes.sink.id, NULL);
    if (!sink)
        g_error(
            "wireplumber_service.c:set_default_nodes() failed to find default "
            "sink "
            "node");

    self->default_nodes.sink.name = wp_pipewire_object_get_property(
        WP_PIPEWIRE_OBJECT(sink), PW_KEY_NODE_DESCRIPTION);

    self->default_nodes.sink.media_class = wp_pipewire_object_get_property(
        WP_PIPEWIRE_OBJECT(sink), PW_KEY_MEDIA_CLASS);

    g_debug(
        "wireplumber_service.c:set_default_sink() settings default Audio Sink "
        "[%s] [%s]"
        "volume: "
        "%.2f mute: %d step: %.2f base: %.2f",
        self->default_nodes.sink.name, self->default_nodes.sink.media_class,
        self->default_nodes.sink.volume, self->default_nodes.sink.mute,
        self->default_nodes.sink.step, self->default_nodes.sink.base);

    g_signal_emit(self, service_signals[default_nodes_change], 0,
                  self->default_nodes.sink.id);
}

static void init_default_sink(WirePlumberService *self) {
    g_debug("wireplumber_service.c:init_default_sink() called");

    g_signal_emit_by_name(self->default_nodes_api, "get-default-node",
                          "Audio/Sink", &self->default_nodes.sink.id);

    set_default_sink(self);
}

static void set_default_source(WirePlumberService *self) {
    GVariant *mixer = NULL;
    WpNode *source = NULL;
    WpNodeState state = {0};

    g_debug("wireplumber_service.c:set_default_source() called");

    g_signal_emit_by_name(self->mixer_api, "get-volume",
                          self->default_nodes.source.id, &mixer);

    g_variant_lookup(mixer, "volume", "d", &self->default_nodes.source.volume);
    g_variant_lookup(mixer, "mute", "b", &self->default_nodes.source.mute);
    g_variant_lookup(mixer, "step", "b", &self->default_nodes.source.step);
    g_variant_lookup(mixer, "base", "d", &self->default_nodes.source.base);

    // convert volume to cubic
    self->default_nodes.source.volume =
        volume_from_linear(self->default_nodes.source.volume, SCALE_CUBIC);

    source = wp_object_manager_lookup(
        self->obj_mgr, WP_TYPE_NODE, WP_CONSTRAINT_TYPE_G_PROPERTY, "bound-id",
        "=u", self->default_nodes.source.id, NULL);
    if (!source)
        g_error(
            "wireplumber_service.c:set_default_nodes() failed to find source "
            "node");

    // if input is running set the state.
    self->default_nodes.source.active = true;
    state = wp_node_get_state(source, NULL);
    if (state != WP_NODE_STATE_RUNNING) {
        self->default_nodes.source.active = false;
    }

    self->default_nodes.source.name = wp_pipewire_object_get_property(
        WP_PIPEWIRE_OBJECT(source), PW_KEY_NODE_DESCRIPTION);

    self->default_nodes.source.media_class = wp_pipewire_object_get_property(
        WP_PIPEWIRE_OBJECT(source), PW_KEY_MEDIA_CLASS);

    g_debug(
        "wireplumber_service.c:set_default_source() setting default Audio "
        "Source [%s] [%s]"
        "volume: %.2f mute: %d step: %.2f base: %.2f active: %d",
        self->default_nodes.source.name, self->default_nodes.source.media_class,
        self->default_nodes.source.volume, self->default_nodes.source.mute,
        self->default_nodes.source.step, self->default_nodes.source.base,
        self->default_nodes.source.active);

    g_signal_emit(self, service_signals[default_nodes_change], 0,
                  self->default_nodes.source.id);
}

static void on_source_state_change(WpNode *node, WpNodeState *old_state,
                                   WpNodeState *new_state,
                                   WirePlumberService *self) {
    set_default_source(self);
}

static void init_default_source(WirePlumberService *self) {
    WpNode *source = NULL;

    g_debug("wireplumber_service.c:init_default_source() called");

    g_signal_emit_by_name(self->default_nodes_api, "get-default-node",
                          "Audio/Source", &self->default_nodes.source.id);

    set_default_source(self);

    source = wp_object_manager_lookup(
        self->obj_mgr, WP_TYPE_NODE, WP_CONSTRAINT_TYPE_G_PROPERTY, "bound-id",
        "=u", self->default_nodes.source.id, NULL);
    if (!source)
        g_error(
            "wireplumber_service.c:set_default_nodes() failed to find source "
            "node");

    // wire up to state change to signal microphone active or not
    g_signal_connect(WP_NODE(source), "state-changed",
                     G_CALLBACK(on_source_state_change), self);
}

static void on_mixer_change(WirePlumberService *self, gint32 id) {
    g_debug("wireplumber_service.c:on_mixer_change() called");

    if (id == self->default_nodes.sink.id) {
        set_default_sink(self);
        return;
    };

    if (id == self->default_nodes.source.id) {
        set_default_source(self);
        return;
    };
}

static void set_default_nodes(WirePlumberService *self) {
    g_debug("wireplumber_service.c:set_default_nodes() called");
    init_default_sink(self);
    init_default_source(self);
};

static void on_installed(WirePlumberService *self) {
    g_info(
        "wireplumber_service.c:wire_plumber_service_init() WirePlumberService "
        "initialized "
        "Name: %s Version: %s ",
        wp_core_get_remote_name(self->core),
        wp_core_get_remote_version(self->core));

    // set default nodes
    set_default_nodes(self);

    // connect to mixer api changed event
    g_signal_connect_swapped(self->mixer_api, "changed",
                             G_CALLBACK(on_mixer_change), self);
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
        wp_core_install_object_manager(self->core, self->obj_mgr);
}

static void wire_plumber_service_init(WirePlumberService *self) {
    GError *error = NULL;

    g_debug("wireplumber_service.c:wire_plumber_service_init() called");
    wp_init(WP_INIT_PIPEWIRE);

    self->core = wp_core_new(NULL, NULL);
    self->obj_mgr = wp_object_manager_new();
    self->pending_plugins = 2;

    wp_object_manager_add_interest(self->obj_mgr, WP_TYPE_NODE, NULL);
    wp_object_manager_request_object_features(self->obj_mgr, WP_TYPE_NODE,
                                              WP_PIPEWIRE_OBJECT_FEATURES_ALL);

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
    g_signal_connect_swapped(self->obj_mgr, "installed",
                             G_CALLBACK(on_installed), self);

    // activate plugins
    wp_object_activate(WP_OBJECT(self->default_nodes_api),
                       WP_PLUGIN_FEATURE_ENABLED, NULL,
                       (GAsyncReadyCallback)on_plugin_activate, self);

    wp_object_activate(WP_OBJECT(self->mixer_api), WP_PLUGIN_FEATURE_ENABLED,
                       NULL, (GAsyncReadyCallback)on_plugin_activate, self);
};

void wire_plumber_service_get_default_nodes(
    WirePlumberService *self, WirePlumberServiceDefaultNodes *out) {
    g_debug(
        "wireplumber_service.c:wire_plumber_service_get_default_nodes() "
        "called");
    *out = self->default_nodes;
    return;
};

void wire_plumber_service_default_nodes_req(WirePlumberService *self) {
    g_debug(
        "wireplumber_service.c:wire_plumber_service_default_nodes_req() "
        "called");
    g_signal_emit(self, service_signals[default_nodes_change], 0, 0);
};

int wire_plumber_service_global_init() {
    g_debug("wireplumber_service.c:wire_plumber_service_global_init() called");
    if (global == NULL) {
        global = g_object_new(WIRE_PLUMBER_SERVICE_TYPE, NULL);
    }
    return 0;
}

WirePlumberService *wire_plumber_service_get_global() { return global; }