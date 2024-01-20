#include <adwaita.h>

#include "quick_settings_grid_button.h"

enum QuickSettingsGridClusterSide {
    QUICK_SETTINGS_GRID_CLUSTER_NONE,
    QUICK_SETTINGS_GRID_CLUSTER_BOTH,
    QUICK_SETTINGS_GRID_CLUSTER_LEFT,
    QUICK_SETTINGS_GRID_CLUSTER_RIGHT,
};

// Callback function invoked when a QuickSettingCluster revealed the widget
// associated with a QuickSettingsGridButton's reveal button click.
typedef void (*QuickSettingsClusterOnRevealFunc)(
    QuickSettingsGridButton *revealed, gboolean is_revealed);

G_BEGIN_DECLS

// A QuickSettingsGridCluster consists of two QuickSettingsGridButtons layed out
// side-by-side in a GtkCenterBox.
//
// The GtkCenterBox is placed on top of two GtkRevealers, one for the left, and
// one for the right.
//
// If either QuickSettingsGridButton has a 'reveal' button it will be wired to
// the appropriate GtkRevealer on it's click.
struct _QuickSettingsGridCluster;
#define QUICK_SETTINGS_GRID_CLUSTER_TYPE quick_settings_grid_cluster_get_type()
G_DECLARE_FINAL_TYPE(QuickSettingsGridCluster, quick_settings_grid_cluster,
                     QUICK_SETTINGS_GRID, CLUSTER, GObject);

G_END_DECLS

GtkWidget *quick_settings_grid_cluster_get_widget(
    QuickSettingsGridCluster *cluster);

int quick_settings_grid_cluster_add_button(
    QuickSettingsGridCluster *cluster, enum QuickSettingsGridClusterSide side,
    QuickSettingsGridButton *button, GtkWidget *reveal,
    QuickSettingsClusterOnRevealFunc on_reveal);

gboolean quick_settings_grid_cluster_is_full(QuickSettingsGridCluster *cluster);

enum QuickSettingsGridClusterSide quick_settings_grid_cluster_vacant_side(
    QuickSettingsGridCluster *cluster);

GtkCenterBox *quick_settings_grid_cluster_get_center_box(
    QuickSettingsGridCluster *cluster);

GtkRevealer *quick_settings_grid_cluster_get_left_revealer(
    QuickSettingsGridCluster *cluster);

GtkRevealer *quick_settings_grid_cluster_get_right_revealer(
    QuickSettingsGridCluster *cluster);

void quick_settings_grid_cluster_slide_left(QuickSettingsGridCluster *cluster);

void quick_settings_grid_cluster_hide_all(QuickSettingsGridCluster *cluster);
