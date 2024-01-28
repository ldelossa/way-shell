#pragma once
#include <adwaita.h>
#include <sys/cdefs.h>
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

// Indexes for the WirePlumberServicePorts array
enum WirePlumberServicePortChannel {
        WIRE_PLUMBER_SERVICE_PORT_RL,
        WIRE_PLUMBER_SERVICE_PORT_RR,
        WIRE_PLUMBER_SERVICE_PORT_FL,
        WIRE_PLUMBER_SERVICE_PORT_FR,
        WIRE_PLUMBER_SERVICE_PORT_C,
        WIRE_PLUMBER_SERVICE_PORT_LFE,
        WIRE_PLUMBER_SERVICE_PORT_SL,
        WIRE_PLUMBER_SERVICE_PORT_SR,
        WIRE_PLUMBER_SERVICE_PORT_RHL,
        WIRE_PLUMBER_SERVICE_PORT_RHR,
        WIRE_PLUMBER_SERVICE_PORT_TFL,
        WIRE_PLUMBER_SERVICE_PORT_TFR
    };

    static __always_inline enum WirePlumberServicePortChannel
    wire_plumber_service_map_port(const char *channel) {
        if (g_strcmp0(channel, "FL") == 0) {
            return WIRE_PLUMBER_SERVICE_PORT_FL;
        } else if (g_strcmp0(channel, "FR") == 0) {
            return WIRE_PLUMBER_SERVICE_PORT_FR;
        } else if (g_strcmp0(channel, "RL") == 0) {
            return WIRE_PLUMBER_SERVICE_PORT_RL;
        } else if (g_strcmp0(channel, "RR") == 0) {
            return WIRE_PLUMBER_SERVICE_PORT_RR;
        } else if (g_strcmp0(channel, "C") == 0) {
            return WIRE_PLUMBER_SERVICE_PORT_C;
        } else if (g_strcmp0(channel, "LFE") == 0) {
            return WIRE_PLUMBER_SERVICE_PORT_LFE;
        } else if (g_strcmp0(channel, "SL") == 0) {
            return WIRE_PLUMBER_SERVICE_PORT_SL;
        } else if (g_strcmp0(channel, "SR") == 0) {
            return WIRE_PLUMBER_SERVICE_PORT_SR;
        } else if (g_strcmp0(channel, "RHL") == 0) {
            return WIRE_PLUMBER_SERVICE_PORT_RHL;
        } else if (g_strcmp0(channel, "RHR") == 0) {
            return WIRE_PLUMBER_SERVICE_PORT_RHR;
        } else if (g_strcmp0(channel, "TFL") == 0) {
            return WIRE_PLUMBER_SERVICE_PORT_TFL;
        } else if (g_strcmp0(channel, "TFR") == 0) {
            return WIRE_PLUMBER_SERVICE_PORT_TFR;
        }
        return -1;
}

// Defines a fixed size array which holds in input or output ports of a
// device.
//
// The array is large enough to accommodate a 9.1.2 speaker system.
// The array ports are re-used for capture devices, where they express the
// microphone's location.
//
// This array only holds the "primary" ports for the node.
// This means if the node is an input device the ports will be the input ports
// (not monitors or other ports).
//
// Likewise if the device is an output device the ports will be the output ports
// the data is trasmitted on.
//
// [ RL, RR, FL, FR, C, LFE, SL, SR, RHL, RHR, TFL, TFR ]
typedef int WirePlumberServicePorts[12];

// This is a header type which any pointer to a WirePlumberServiceNode type can
// be casted to to discover the type.
//
// Any struct which is derived from this must always include these two values
// as their first members. We don't embed this struct just to avoid typing
// .header.* everywhere.
typedef struct _WirePlumberServiceNodeHeader {
    enum WirePlumberServiceType type;
    guint32 id;
    WirePlumberServicePorts ports;
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
    guint32 id;
    WirePlumberServicePorts ports;
    const gchar *name;
    const gchar *media_class;
    const gchar *nick_name;
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
    guint32 id;
    WirePlumberServicePorts ports;
    const gchar *name;
    const gchar *app_name;
    const gchar *media_class;
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
    guint32 input_node;
    guint32 output_node;
    guint32 input_port;
    guint32 output_port;
} WirePlumberServiceLink;

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

void wire_plumber_service_set_link(WirePlumberService *self,
                                   WirePlumberServiceNodeHeader *output,
                                   WirePlumberServiceNodeHeader *input);

gboolean wire_plumber_service_microphone_active(WirePlumberService *self);

char *wire_plumber_service_map_source_vol_icon(float vol, gboolean mute);

char *wire_plumber_service_map_sink_vol_icon(float vol, gboolean mute);

GHashTable *wire_plumber_service_get_db(WirePlumberService *self);