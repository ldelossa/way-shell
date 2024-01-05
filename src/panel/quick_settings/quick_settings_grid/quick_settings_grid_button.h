#pragma once

#include <adwaita.h>

enum QuickSettingsButtonType {
    QUICK_SETTINGS_BUTTON_GENERIC,
    QUICK_SETTINGS_BUTTON_ETHERNET,
    QUICK_SETTINGS_BUTTON_WIFI,
};

typedef struct _QuickSettingsGridCluster QuickSettingsGridCluster;

typedef struct _QuickSettingsGridButton {
    enum QuickSettingsButtonType type;
    GtkBox *container;
    GtkButton *button;
    GtkLabel *title;
    GtkLabel *subtitle;
    GtkImage *icon;
    GtkButton *reveal;
    QuickSettingsGridCluster *cluster;
} QuickSettingsGridButton;

QuickSettingsGridButton *quick_settings_grid_button_new(
    enum QuickSettingsButtonType type, const gchar *title,
    const gchar *subtitle, const gchar *icon_name, gboolean with_reveal);

void quick_settings_grid_button_init(QuickSettingsGridButton *self,
                                     enum QuickSettingsButtonType type,
                                     const gchar *title, const gchar *subtitle,
                                     const gchar *icon_name,
                                     gboolean with_reveal);

void quick_settings_grid_button_free(QuickSettingsGridButton *self);