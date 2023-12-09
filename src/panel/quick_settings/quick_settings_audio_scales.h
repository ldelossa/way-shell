#pragma once

#include <adwaita.h>

typedef struct _AudioScales {
    GtkBox *container;
    struct {
        GtkBox *container;
        GtkImage *icon;
        GtkScale *scale;
    } default_sink;
    struct {
        GtkBox *container;
        GtkImage *icon;
        GtkScale *scale;
    } default_source;
} AudioScales;

void quick_settings_audio_scales_init(AudioScales *scales);

void quick_settings_audio_scales_free(AudioScales *scales);