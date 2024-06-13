#include "quick_settings_grid.h"

#include <adwaita.h>

#include "../../../services/logind_service/logind_service.h"
#include "../../../services/network_manager_service.h"
#include "../../../services/power_profiles_service/power_profiles_service.h"
#include "./quick_settings_grid_airplane_mode_button.h"
#include "./quick_settings_grid_night_light/quick_settings_grid_night_light.h"
#include "./quick_settings_grid_power_profiles/quick_settings_grid_power_profiles.h"
#include "./quick_settings_grid_wifi/quick_settings_grid_wifi.h"
#include "nm-dbus-interface.h"
#include "quick_settings_grid_button.h"
#include "quick_settings_grid_cluster.h"
#include "quick_settings_grid_ethernet.h"
#include "quick_settings_grid_idle_inhibitor.h"
#include "quick_settings_grid_oneshot_button.h"
#include "quick_settings_grid_theme.h"

enum signals { signals_n };

typedef struct _QuickSettingsGrid {
    GObject parent_instance;
    GtkBox *container;
    // An array of QuickSettingsGridClusters.
    // The of the array cooresponds to the order of the displayed cluster in the
    // GUI.
    GPtrArray *clusters;
    // An array of NMDevices.
    // If an NMDevice is in this list a QuickSettingGridButton exists for it
    // and is watching the device's status.
    GPtrArray *managed_network_devices;
    gboolean compacting;
} QuickSettingsGrid;

G_DEFINE_TYPE(QuickSettingsGrid, quick_settings_grid, G_TYPE_OBJECT);

static void clean_empty_cluster(QuickSettingsGridCluster *cluster,
                                QuickSettingsGrid *self) {
    g_debug("quick_settings_grid.c:on_cluster_empty() called");
    gtk_box_remove(self->container,
                   quick_settings_grid_cluster_get_widget(cluster));
    g_ptr_array_remove(self->clusters, cluster);
    g_object_unref(cluster);
}

static void on_will_reveal(QuickSettingsGridCluster *cluster,
                           QuickSettingsGridButton *button,
                           QuickSettingsGrid *self) {
    g_debug("quick_settings_grid.c:on_will_reveal() called");
    // when a grid cluster informs us it will reveal a child, we want to ask
    // all other clusters to hide theirs.
    for (int i = 0; i < self->clusters->len; i++) {
        QuickSettingsGridCluster *tmp = g_ptr_array_index(self->clusters, i);
        if (tmp != cluster) {
            quick_settings_grid_cluster_hide_all(tmp);
        }
    }
}

static void compact_grid_on_remove(QuickSettingsGridCluster *cluster,
                                   enum QuickSettingsGridClusterSide side,
                                   QuickSettingsGrid *self) {
    g_debug("quick_settings_grid.c:compact_grid_on_remove_button_req() called");

    // no need to perform any nested compaction.
    if (self->compacting) return;

    self->compacting = true;

    // we only need to consider compaction if we have more then one cluster.
    if (self->clusters->len <= 1) {
        self->compacting = false;
        return;
    }

    for (int i = 0; i < self->clusters->len - 1; i++) {
        QuickSettingsGridCluster *cluster =
            g_ptr_array_index(self->clusters, i);
        QuickSettingsGridCluster *next_cluster =
            g_ptr_array_index(self->clusters, i + 1);
        enum QuickSettingsGridClusterSide vacant_side = -1;

        vacant_side = quick_settings_grid_cluster_vacant_side(cluster);
        if (vacant_side != QUICK_SETTINGS_GRID_CLUSTER_RIGHT) continue;

        // move next_cluster's left button to right
        QuickSettingsGridButton *next_left =
            quick_settings_grid_cluster_get_left_button(next_cluster);
        quick_settings_grid_cluster_remove_button(next_cluster, next_left);

        // add it to cluster's right side.
        quick_settings_grid_cluster_add_button(
            cluster, QUICK_SETTINGS_GRID_CLUSTER_RIGHT, next_left);
    }

    // prune any empty clusters
    for (int i = self->clusters->len - 1; i < self->clusters->len; i--) {
        QuickSettingsGridCluster *cluster =
            g_ptr_array_index(self->clusters, i);

        enum QuickSettingsGridClusterSide vacant_side =
            quick_settings_grid_cluster_vacant_side(cluster);

        if (vacant_side == QUICK_SETTINGS_GRID_CLUSTER_BOTH) {
            clean_empty_cluster(cluster, self);
        }
    }

    self->compacting = false;
}

static void quick_settings_grid_add_button(QuickSettingsGrid *self,
                                           QuickSettingsGridButton *button) {
    QuickSettingsGridCluster *cluster = NULL;

    g_debug("quick_settings_grid.c:add_button() called");

    enum QuickSettingsGridClusterSide side = QUICK_SETTINGS_GRID_CLUSTER_NONE;

    // find a cluster which isn't full.
    if (self->clusters->len > 0) {
        for (int i = 0; i < self->clusters->len; i++) {
            QuickSettingsGridCluster *tmp =
                g_ptr_array_index(self->clusters, i);
            if (!quick_settings_grid_cluster_is_full(tmp)) {
                cluster = g_ptr_array_index(self->clusters, i);
                side = quick_settings_grid_cluster_vacant_side(cluster);
                break;
            }
        }
    }

    // we couldn't find a cluster with an empty slot for our button, create it.
    if (!cluster) {
        cluster = g_object_new(QUICK_SETTINGS_GRID_CLUSTER_TYPE, NULL);

        // attach to cluster's will reveal so we can ask other clusters to hide
        // their widgets.
        g_signal_connect(cluster, "will-reveal", G_CALLBACK(on_will_reveal),
                         self);

        side = QUICK_SETTINGS_GRID_CLUSTER_LEFT;
        gtk_box_append(self->container,
                       quick_settings_grid_cluster_get_widget(cluster));
        g_ptr_array_add(self->clusters, cluster);

        g_signal_connect(cluster, "removed", G_CALLBACK(compact_grid_on_remove),
                         self);
    }

    // the cluster owns this button now, will notify us when it is removed,
    // and will clean the button's memory.
    quick_settings_grid_cluster_add_button(cluster, side, button);

    // adjust button sizes.
    quick_settings_grid_cluster_realize_size(cluster);
}

static void on_network_manager_change(NetworkManagerService *nm,
                                      QuickSettingsGrid *self) {
    const GPtrArray *devices = network_manager_service_get_devices(nm);

    g_debug("quick_settings_grid.c:on_network_manager_change() called");

    for (int i = 0; i < devices->len; i++) {
        NMDevice *dev = g_ptr_array_index(devices, i);
        NMDevice *found = NULL;
        NMDeviceType type = nm_device_get_device_type(dev);
        NMDeviceState state = nm_device_get_state(dev);

        // ignore creating buttons for unmanaged devices.
        if (state == NM_DEVICE_STATE_UNMANAGED) continue;

        if (type == NM_DEVICE_TYPE_WIFI || type == NM_DEVICE_TYPE_ETHERNET) {
            found = dev;
        }
        if (!found) continue;

        for (int i = 0; i < self->managed_network_devices->len; i++) {
            dev = g_ptr_array_index(self->managed_network_devices, i);
            if (found == dev) goto next_dev;
        }

        g_ptr_array_add(self->managed_network_devices, found);
        g_object_ref(found);

        if (type == NM_DEVICE_TYPE_WIFI) {
            QuickSettingsGridWifiButton *wifi_button =
                quick_settings_grid_wifi_button_init(NM_DEVICE_WIFI(found));
            quick_settings_grid_add_button(
                self, (QuickSettingsGridButton *)wifi_button);
        }
        if (type == NM_DEVICE_TYPE_ETHERNET) {
            QuickSettingsGridEthernetButton *ethernet_button =
                quick_settings_grid_ethernet_button_init(
                    NM_DEVICE_ETHERNET(found));
            quick_settings_grid_add_button(
                self, (QuickSettingsGridButton *)ethernet_button);
        }
    next_dev:;
    }
}

// stub out dispose, finalize, class_init, and init methods
static void quick_settings_grid_dispose(GObject *gobject) {
    QuickSettingsGrid *self = QUICK_SETTINGS_GRID(gobject);

    // remove ourselves from signals
    NetworkManagerService *nm = network_manager_service_get_global();
    g_signal_handlers_disconnect_by_func(nm, on_network_manager_change, self);

    // free clusters
    for (int i = 0; i < self->clusters->len; i++) {
        QuickSettingsGridCluster *cluster =
            g_ptr_array_index(self->clusters, i);

        // remove from will-reveal signals
        g_signal_handlers_disconnect_by_func(cluster, on_will_reveal, self);

        g_object_unref(cluster);
    }

    // Chain-up
    G_OBJECT_CLASS(quick_settings_grid_parent_class)->dispose(gobject);
};

static void quick_settings_grid_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(quick_settings_grid_parent_class)->finalize(gobject);
};

static void quick_settings_grid_class_init(QuickSettingsGridClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = quick_settings_grid_dispose;
    object_class->finalize = quick_settings_grid_finalize;
};

static void quick_settings_grid_init_layout(QuickSettingsGrid *self) {
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_hexpand(GTK_WIDGET(self->container), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(self->container), TRUE);
    gtk_widget_set_name(GTK_WIDGET(self->container), "quick-settings-grid");

    NetworkManagerService *nm = network_manager_service_get_global();
    on_network_manager_change(nm, self);

    // if power profiles service available create button for it
    PowerProfilesService *pps = power_profiles_service_get_global();
    if (pps) {
        QuickSettingsGridPowerProfilesButton *pps_button =
            quick_settings_grid_power_profiles_button_init();
        quick_settings_grid_add_button(self,
                                       (QuickSettingsGridButton *)pps_button);
    }

    // if logind service is available add inhibitor button
    LogindService *logind = logind_service_get_global();
    if (logind) {
        QuickSettingsGridIdleInhibitorButton *inhibitor_button =
            quick_settings_grid_idle_inhibitor_button_init();
        quick_settings_grid_add_button(
            self, (QuickSettingsGridButton *)inhibitor_button);
    }

    // add night light button
    QuickSettingsGridNightLightButton *night_light_button =
        quick_settings_grid_night_light_button_init();
    quick_settings_grid_add_button(
        self, (QuickSettingsGridButton *)night_light_button);

    // add airplane mode button
    QuickSettingsGridAirplaneModeButton *airplane_mode_button =
        quick_settings_grid_airplane_mode_button_init();
    quick_settings_grid_add_button(
        self, (QuickSettingsGridButton *)airplane_mode_button);

    // add theme button
    QuickSettingsGridOneThemeButton *theme_button =
        quick_settings_grid_theme_button_init();
    quick_settings_grid_add_button(self,
                                   (QuickSettingsGridButton *)theme_button);

    // setup change signal
    g_signal_connect(nm, "changed", G_CALLBACK(on_network_manager_change),
                     self);
};

void quick_settings_grid_reinitialize(QuickSettingsGrid *self) {
    // destroy signals
    NetworkManagerService *nm = network_manager_service_get_global();
    g_signal_handlers_disconnect_by_func(nm, on_network_manager_change, self);

    // free clusters
    for (int i = 0; i < self->clusters->len; i++) {
        QuickSettingsGridCluster *cluster =
            g_ptr_array_index(self->clusters, i);
        g_object_unref(cluster);
    }

    // reset clusters size to 0
    // the clusters inside the array have already been disposed if we are here.
    g_ptr_array_set_size(self->clusters, 0);

    // unref any devices in managed devices
    for (int i = 0; i < self->managed_network_devices->len; i++) {
        g_object_unref(g_ptr_array_index(self->managed_network_devices, i));
    }
    // set managed network devices size to zero
    g_ptr_array_set_size(self->managed_network_devices, 0);

    // reinitialize
    quick_settings_grid_init_layout(self);
}

static void quick_settings_grid_init(QuickSettingsGrid *self) {
    self->clusters = g_ptr_array_new();
    self->managed_network_devices = g_ptr_array_new();
    quick_settings_grid_init_layout(self);
};

GtkWidget *quick_settings_grid_get_widget(QuickSettingsGrid *grid) {
    return GTK_WIDGET(grid->container);
};
