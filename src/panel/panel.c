#include "panel.h"

#include <adwaita.h>
#include <gdk/wayland/gdkwayland.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

#include "../activities/activities.h"
#include "./indicator_bar/indicator_bar.h"
#include "./panel_status_bar/panel_status_bar.h"
#include "panel_clock.h"
#include "panel_mediator.h"
#include "panel_workspaces_bar/panel_workspaces_bar.h"

// Provides a mapping between monitors and valid Panels.
// Keys are GdkMonitor pointers while values are *Panel pointers.
//
// The Panel subsystem reconciles GdkMonitor events such that changes to
// monitors (new/removed) also destroy/create Panels.
static GHashTable *panels = NULL;
// An array of GdkMonitor pointers which mirrors the GListModel returned
// from gdk_display_get_monitors(display);
static GPtrArray *monitors = NULL;

// The global PanelMediator returned by this module when
// panel_get_global_mediator() is called.
static PanelMediator *mediator = NULL;

struct _Panel {
    GObject parent_instance;
    AdwWindow *win;
    GdkMonitor *monitor;
    GtkCenterBox *container;
    GtkBox *left;
    GtkBox *center;
    GtkBox *right;
    PanelWorkspacesBar *ws_bar;
    PanelClock *clock;
    PanelStatusBar *status_bar;
    IndicatorBar *indicator_bar;

    // used to track monitor position in GListModel for deletion.
    guint32 monitor_pos;
    gchar *monitor_desc;
};
G_DEFINE_TYPE(Panel, panel, G_TYPE_OBJECT)

static void on_overlay_visible(Activities *a, Panel *self) {
    g_debug("panel.c:on_activites_visible() called.");
    gtk_widget_add_css_class(GTK_WIDGET(self->container), "activities-visible");
}

static void on_overlay_hidden(Activities *a, Panel *self) {
    g_debug("panel.c:on_activites_hidden() called.");
    gtk_widget_remove_css_class(GTK_WIDGET(self->container),
                                "activities-visible");
}

static void panel_dispose(GObject *gobject) {
    Panel *self = PANEL_PANEL(gobject);

    // kill signals
    Activities *a = activities_get_global();
    g_signal_handlers_disconnect_by_func(a, on_overlay_visible, self);
    g_signal_handlers_disconnect_by_func(a, on_overlay_hidden, self);

    g_object_unref(self->status_bar);
    g_object_unref(self->ws_bar);
    g_object_unref(self->monitor);
    g_object_unref(self->clock);
	g_object_unref(self->indicator_bar);

    g_free(self->monitor_desc);

    // Chain-up
    G_OBJECT_CLASS(panel_parent_class)->dispose(gobject);
};

static void panel_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(panel_parent_class)->finalize(gobject);
};

static void panel_class_init(PanelClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = panel_dispose;
    object_class->finalize = panel_finalize;
};

static void panel_init_workspaces_bar(Panel *self) {
    self->ws_bar = g_object_new(PANEL_WORKSPACES_BAR_TYPE, NULL);
    panel_workspaces_bar_set_panel(self->ws_bar, self);
    gtk_box_append(self->left,
                   GTK_WIDGET(panel_workspaces_bar_get_widget(self->ws_bar)));
}

static void panel_init_panel_clock(Panel *self) {
    self->clock = g_object_new(PANEL_CLOCK_TYPE, NULL);
    gtk_box_append(self->center, panel_clock_get_widget(self->clock));
    gtk_widget_set_halign(GTK_WIDGET(self->center), GTK_ALIGN_CENTER);
    self->clock = self->clock;
    panel_clock_set_panel(self->clock, self);
}

static void panel_init_status_bar(Panel *self) {
    self->status_bar = g_object_new(PANEL_STATUS_BAR_TYPE, NULL);
    panel_status_bar_set_panel(self->status_bar, self);
    gtk_box_append(self->right,
                   GTK_WIDGET(panel_status_bar_get_widget(self->status_bar)));
}

static void panel_init_indicator_bar(Panel *self) {
    self->indicator_bar = g_object_new(INDICATOR_BAR_TYPE, NULL);
    indicator_bar_set_panel(self->indicator_bar, self);
    gtk_box_prepend(self->right, indicator_bar_get_widget(self->indicator_bar));
}

static void panel_init_layout(Panel *self) {
    // NOTE:
    // We do not initialize any of the Panel's dependecies (panel clock,
    // workspaces bar, etc...) until 'panel_attach_to_monitor' since the
    // depedencies expect the panel to always have a valid monitor.
    self->win = ADW_WINDOW(adw_window_new());
    gtk_layer_init_for_window(GTK_WINDOW(self->win));
    gtk_layer_set_namespace(GTK_WINDOW(self->win), "way-shell-panel");
    gtk_layer_set_layer((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_auto_exclusive_zone_enable(GTK_WINDOW(self->win));
    gtk_layer_set_anchor(GTK_WINDOW(self->win), 0, 1);
    gtk_layer_set_anchor(GTK_WINDOW(self->win), 1, 1);
    gtk_layer_set_anchor(GTK_WINDOW(self->win), 2, 1);
    gtk_layer_set_anchor(GTK_WINDOW(self->win), 3, 0);
    gtk_widget_set_size_request(GTK_WIDGET(self->win), -1, 30);

    self->container = GTK_CENTER_BOX(gtk_center_box_new());
    gtk_widget_set_name(GTK_WIDGET(self->container), "panel");
    self->left = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    self->center = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    self->right = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

    gtk_center_box_set_start_widget(self->container, GTK_WIDGET(self->left));
    gtk_center_box_set_center_widget(self->container, GTK_WIDGET(self->center));
    gtk_center_box_set_end_widget(self->container, GTK_WIDGET(self->right));

    adw_window_set_content(ADW_WINDOW(self->win), GTK_WIDGET(self->container));
}

static void panel_init(Panel *self) { panel_init_layout(self); };

// Attach the Panel to the GtkApplication and fix it to the provided GdkMonitor
void panel_attach_to_monitor(Panel *self, GdkMonitor *monitor) {
    self->monitor = monitor;

    gtk_layer_set_monitor(GTK_WINDOW(self->win), monitor);
    g_hash_table_insert(panels, monitor, self);
    gtk_window_present(GTK_WINDOW(self->win));

    panel_init_workspaces_bar(self);
    panel_init_panel_clock(self);
    panel_init_status_bar(self);
    panel_init_indicator_bar(self);

    Activities *a = activities_get_global();

    // listen for activities-visible
    g_signal_connect(a, "activities-will-show", G_CALLBACK(on_overlay_visible),
                     self);
    // listen for activities-hidden
    g_signal_connect(a, "activities-will-hide", G_CALLBACK(on_overlay_hidden),
                     self);
}

static void panel_on_monitor_added(GdkMonitor *mon, guint pos) {
    const char *desc = NULL;

    g_debug("panel.c:panel_on_monitor_added(): called.");

    if (!gdk_monitor_is_valid(mon)) {
        g_warning(
            "panel.c:panel_on_monitor_change() received invalid monitor "
            "from "
            "Gdk, moving to next.");
        return;
    }

    // insert into our mirrored monitor list.
    g_ptr_array_insert(monitors, pos, mon);

    Panel *panel = g_object_new(PANEL_TYPE, NULL);
    panel->monitor_desc = g_strdup(desc);
    panel_attach_to_monitor(panel, mon);

    g_debug("panel.c:panel_on_monitor_added(): added bar for monitor: [%s]",
            panel->monitor_desc);
}

static void panel_on_monitor_removed(guint pos) {
    g_debug("panel.c:panel_on_monitor_removed(): called.");

    GdkMonitor *removed = g_ptr_array_index(monitors, pos);
    if (!removed) return;
    g_ptr_array_remove_index(monitors, pos);

    Panel *panel = g_hash_table_lookup(panels, removed);
    if (!panel) return;

    g_debug(
        "panel.c:panel_on_monitor_removed(): removing bar for monitor: [%s]",
        panel->monitor_desc);

    g_object_unref(panel);

    if (GTK_IS_WINDOW(panel->win)) gtk_window_destroy(GTK_WINDOW(panel->win));

    g_hash_table_remove(panels, removed);
}

// Iterates over the monitors GListModel, creating panels for any new monitors.
// Designed to be a handler for the GListModel(GdkMonitor)::items-changed
// signal.
static void panel_on_monitor_change(GListModel *monitors, guint position,
                                    guint removed, guint added,
                                    gpointer gtk_app) {
    uint8_t n = g_list_model_get_n_items(monitors);

    // debug all arguments and n
    g_debug(
        "panel.c:panel_on_monitor_change() called with position: [%d], "
        "removed: [%d], added: [%d], n: [%d]",
        position, removed, added, n);

    if (added > 0) {
        panel_on_monitor_added(g_list_model_get_item(monitors, position),
                               position);
    }

    if (removed > 0) {
        panel_on_monitor_removed(position);
    }
}

PanelMediator *panel_get_global_mediator() { return mediator; }

GdkMonitor *panel_get_monitor(Panel *panel) { return panel->monitor; }

int panel_get_height(Panel *panel) {
    return gtk_widget_get_height(GTK_WIDGET(panel->container));
}

void panel_on_msg_tray_visible(Panel *panel) {
    g_debug("panel.c:panel_on_msg_tray_visible() called.");
    panel_clock_set_toggled(panel->clock, TRUE);
}

void panel_on_msg_tray_hidden(Panel *panel) {
    g_debug("panel.c:panel_on_msg_tray_hidden() called.");
    panel_clock_set_toggled(panel->clock, FALSE);
}

void panel_on_qs_visible(Panel *panel) {
    g_debug("panel.c:panel_on_qs_visible() called.");
    panel_status_bar_set_toggled(panel->status_bar, TRUE);
}

void panel_on_qs_hidden(Panel *panel) {
    g_debug("panel.c:panel_on_qs_hidden() called.");
    panel_status_bar_set_toggled(panel->status_bar, FALSE);
}

void panel_activate(AdwApplication *app, gpointer user_data) {
    mediator = g_object_new(PANEL_MEDIATOR_TYPE, NULL);
    panels = g_hash_table_new(g_direct_hash, g_direct_equal);
    monitors = g_ptr_array_new();

    g_debug(
        "panel.c:panel_activate() initializing bars with synthetic monitor "
        "event.");

    GdkSeat *seat = gdk_display_get_default_seat(gdk_display_get_default());
    GdkDisplay *display = gdk_seat_get_display(seat);
    GListModel *monitors = gdk_display_get_monitors(display);

    for (uint8_t i = 0; i < g_list_model_get_n_items(monitors); i++) {
        panel_on_monitor_added(g_list_model_get_item(monitors, i), i);
    }

    g_debug(
        "panel.c:panel_activate() init finished, listening for monitor "
        "events.");

    // Connect the callback function to the "items-changed" signal
    // so we'll destroy and recreate bars on monitor changes.
    //
    // The GListModel returned from gdk_display_get_monitors should stay valid
    // until GdkDisplay is closed, at which point we are closing all windows
    // anyway, so don't bother tracking connection ID for later disconnect.
    g_signal_connect(monitors, "items-changed",
                     G_CALLBACK(panel_on_monitor_change), app);
}

Panel *panel_get_from_monitor(GdkMonitor *monitor) {
    return g_hash_table_lookup(panels, monitor);
}

GHashTable *panel_get_all_panels() { return g_hash_table_ref(panels); }
