#pragma once
#include <adwaita.h>
#include <wireplumber-0.4/wp/wp.h>

enum {
    SCALE_LINEAR,
    SCALE_CUBIC,
};

enum WirePlumberServiceType {
    WIRE_PLUMBER_SERVICE_TYPE_UNKNOWN,
    WIRE_PLUMBER_SERVICE_TYPE_SINK,
    WIRE_PLUMBER_SERVICE_TYPE_SOURCE,
    WIRE_PLUMBER_SERVICE_TYPE_INPUT_AUDIO_STREAM,
    WIRE_PLUMBER_SERVICE_TYPE_OUTPUT_AUDIO_STREAM,
    WIRE_PLUMBER_SERVICE_TYPE_LINK,
};

// This is a header type which any pointer to a WirePlumberServiceNode type can
// be casted to to discover the type.
typedef struct _WirePlumberServiceNodeHeader {
    enum WirePlumberServiceType type;
} WirePlumberServiceNodeHeader;

static inline gdouble volume_from_linear(float vol, gint scale) {
    if (vol <= 0.0f)
        return 0.0;
    else if (scale == SCALE_CUBIC)
        return cbrt(vol);
    else
        return vol;
}

static inline float volume_to_linear(gdouble vol, gint scale) {
    if (vol <= 0.0f)
        return 0.0;
    else if (scale == SCALE_CUBIC)
        return vol * vol * vol;
    else
        return vol;
}

// A Pipewire Node inventoried by the WirePlumberService.
typedef struct WirePlumberServiceNode {
    enum WirePlumberServiceType type;
    const gchar *name;
    const gchar *media_class;
    const gchar *nick_name;
    guint32 id;
    gdouble volume;
    gboolean mute;
    gboolean active;
    gdouble step;
    gdouble base;
    WpNodeState state;
} WirePlumberServiceNode;

// A Pipewire Node inventoried by the WirePlumberService.
typedef struct WirePlumberServiceAudioStream {
    enum WirePlumberServiceType type;
    const gchar *name;
    const gchar *app_name;
    const gchar *media_class;
    guint32 id;
    gdouble volume;
    gboolean mute;
    gboolean active;
    gdouble step;
    gdouble base;
    WpNodeState state;
} WirePlumberServiceAudioStream;

typedef struct WirePlumberServiceLink {
    enum WirePlumberServiceType type;
    guint32 id;
    guint32 input_node_id;
    guint32 output_node_id;
} WirePlumberServiceLink;

// Provides access to important fields of the currently configured default
// Sink (Audio Output) and Source (Audio Input).
typedef struct WirePlumberServiceDefaultNodes {
    WirePlumberServiceNode sink;
    WirePlumberServiceNode source;
} WirePlumberServiceDefaultNodes;

G_BEGIN_DECLS

// Service which provides power state information for various devices.
// Backend is provided by UPower daemon.
struct _WirePlumberService;
#define WIRE_PLUMBER_SERVICE_TYPE wire_plumber_service_get_type()
G_DECLARE_FINAL_TYPE(WirePlumberService, wire_plumber_service, WIRE_PLUMBER,
                     SERVICE, GObject);

G_END_DECLS

// Initialize the global WirePlumberService instance.
int wire_plumber_service_global_init(void);

// Returns the global WirePlumberService instance.
WirePlumberService *wire_plumber_service_get_global(void);

// Returns the latest default nodes from a WirePlumberService instance.
// Typically called with the WirePlumberService argument in a handler of the
// "default-nodes-changed" signal.
void wire_plumber_service_get_default_nodes(
    WirePlumberService *self, WirePlumberServiceDefaultNodes *out);

// Volume control methods.

void wire_plumber_service_set_volume(WirePlumberService *self,
                                     const WirePlumberServiceNode *node,
                                     float volume);

void wire_plumber_service_volume_up(WirePlumberService *self,
                                    const WirePlumberServiceNode *node);

void wire_plumber_service_volume_down(WirePlumberService *self,
                                      const WirePlumberServiceNode *node);

void wire_plumber_service_volume_mute(WirePlumberService *self,
                                      const WirePlumberServiceNode *node);

void wire_plumber_service_volume_unmute(WirePlumberService *self,
                                        const WirePlumberServiceNode *node);

// Methods

WirePlumberServiceNode *wire_plumber_service_get_default_sink(
    WirePlumberService *self);

WirePlumberServiceNode *wire_plumber_service_get_default_source(
    WirePlumberService *self);

GPtrArray *wire_plumber_service_get_sinks(WirePlumberService *self);

GPtrArray *wire_plumber_service_get_sources(WirePlumberService *self);

GPtrArray *wire_plumber_service_get_streams(WirePlumberService *self);

GPtrArray *wire_plumber_service_get_links(WirePlumberService *self);

gboolean wire_plumber_service_microphone_active(WirePlumberService *self);

char *wire_plumber_service_map_source_vol_icon(float vol, gboolean mute);

char *wire_plumber_service_map_sink_vol_icon(float vol, gboolean mute);

GHashTable *wire_plumber_service_get_db(WirePlumberService *self);