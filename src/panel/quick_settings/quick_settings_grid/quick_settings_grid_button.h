#pragma once

#include <adwaita.h>

enum QuickSettingsButtonType {
    QUICK_SETTINGS_BUTTON_GENERIC,
    QUICK_SETTINGS_BUTTON_ONESHOT,
    QUICK_SETTINGS_BUTTON_ETHERNET,
    QUICK_SETTINGS_BUTTON_WIFI,
    QUICK_SETTINGS_BUTTON_PERFORMANCE,
};

typedef struct _QuickSettingsGridCluster QuickSettingsGridCluster;
typedef struct _QuickSettingsGridButton QuickSettingsGridButton;

// Callback function invoked when a QuickSettingCluster revealed the widget
// associated with a QuickSettingsGridButton's reveal button click.
typedef void (*QuickSettingsClusterOnRevealFunc)(
    QuickSettingsGridButton *revealed, gboolean is_revealed);

typedef struct _QuickSettingsGridButton {
    QuickSettingsGridCluster *cluster;
    enum QuickSettingsButtonType type;
    GtkBox *container;
    GtkButton *toggle;
    GtkLabel *title;
    GtkLabel *subtitle;
    GtkImage *icon;
    GtkButton *reveal;
    GtkWidget *reveal_widget;
    QuickSettingsClusterOnRevealFunc on_reveal;
} QuickSettingsGridButton;

QuickSettingsGridButton *quick_settings_grid_button_new(
    enum QuickSettingsButtonType type, const gchar *title,
    const gchar *subtitle, const gchar *icon_name, GtkWidget *reveal_widget,
    QuickSettingsClusterOnRevealFunc on_reveal);

void quick_settings_grid_button_init(
    QuickSettingsGridButton *self, enum QuickSettingsButtonType type,
    const gchar *title, const gchar *subtitle, const gchar *icon_name,
    GtkWidget *reveal_widget, QuickSettingsClusterOnRevealFunc on_reveal);

void quick_settings_grid_button_free(QuickSettingsGridButton *self);