#include "panel.h"
#include "panel_mediator.h"

#include <adwaita.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

struct _Panel {
    GObject parent_instance;
    AdwWindow *win;
    GdkMonitor *monitor;
    GtkBox *container;
    GtkBox *left;
    GtkBox *center;
    GtkBox *right;
};
G_DEFINE_TYPE(Panel, panel, G_TYPE_OBJECT)

static void panel_dispose(GObject *gobject) {
    Panel *self = PANEL_PANEL(gobject);

    // will unref all children widgets.
    gtk_window_destroy((GTK_WINDOW(self->win)));
    g_clear_object(&self->monitor);

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

static void panel_init(Panel *self) {
    self->win = ADW_WINDOW(adw_window_new());

    // define widget layout.
    // [[left] [center] [right]]
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    self->left = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    self->center = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    self->right = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

    adw_window_set_content(ADW_WINDOW(self->win), NULL);
    gtk_box_append(self->container, GTK_WIDGET(self->left));
    gtk_box_append(self->container, GTK_WIDGET(self->center));
    gtk_box_append(self->container, GTK_WIDGET(self->right));

    gtk_widget_set_hexpand(GTK_WIDGET(self->center), TRUE);

    adw_window_set_content(ADW_WINDOW(self->win), GTK_WIDGET(self->container));
};

// Provides a mapping between monitors and valid Panels.
// Keys are GdkMonitor pointers while values are *Panel pointers.
//
// The Panel subsystem reconciles GdkMonitor events such that changes to
// monitors (new/removed) also destroy/create Panels.
GHashTable *panels = NULL;

// Iterates over the global list of bars.
// Use @iter variable to reference the current bar of the loop.
#define BARS_ITERATE for (GSList *iter = bars; iter != NULL; iter = iter->next)

// Performs our runtime dispose before unref'ing the Panel.
// Designed as a handler for GdkMonior::invalidate signal.
void _panel_runtime_dispose(GdkMonitor *mon, Panel *panel) {
    const char *desc = gdk_monitor_get_description(mon);
    g_debug("bar.c:_bar_free(): received monitor invalidation event for: %s",
            desc);
    // remove bar from hash table.
    g_hash_table_remove(panels, mon);
    // disconnect handler from monitor
    // unref, should be the only reference to p.
    g_object_unref(panel);
}

void _panel_init_date_time_button(Panel *panel) {
    GtkWidget *button = gtk_button_new_with_label("date and time");
    gtk_box_append(panel->center, button);
    gtk_widget_set_halign(GTK_WIDGET(panel->center), GTK_ALIGN_CENTER);
}

void _panel_init_layout(Panel *panel) {
    panel->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    panel->left = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    panel->center = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    panel->right = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    adw_window_set_content(ADW_WINDOW(panel->win), NULL);
    panel->right = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

    gtk_box_append(panel->container, GTK_WIDGET(panel->left));
    gtk_box_append(panel->container, GTK_WIDGET(panel->center));
    gtk_box_append(panel->container, GTK_WIDGET(panel->right));

    gtk_widget_set_hexpand(GTK_WIDGET(panel->center), TRUE);

    adw_window_set_content(ADW_WINDOW(panel->win),
                           GTK_WIDGET(panel->container));
}

// Attach the Panel to the GtkApplication and fix it to the provided GdkMonitor
void _panel_attach_monitor(Panel *panel, AdwApplication *app,
                           GdkMonitor *monitor) {
    panel->monitor = monitor;
    g_object_ref(panel->monitor);

    gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(panel->win));
    gtk_layer_init_for_window(GTK_WINDOW(panel->win));
    gtk_layer_set_layer((GTK_WINDOW(panel->win)), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_layer(GTK_WINDOW(panel->win), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_auto_exclusive_zone_enable(GTK_WINDOW(panel->win));
    gtk_layer_set_monitor(GTK_WINDOW(panel->win), monitor);

    gtk_window_set_default_size(GTK_WINDOW(panel->win), -1, 30);

    // anchor bar to left and right corners to it is streched across
    // monitor.
    gtk_layer_set_anchor(GTK_WINDOW(panel->win), 0, 1);
    gtk_layer_set_anchor(GTK_WINDOW(panel->win), 1, 1);
    gtk_layer_set_anchor(GTK_WINDOW(panel->win), 2, 1);
    gtk_layer_set_anchor(GTK_WINDOW(panel->win), 3, 0);
}

// Further Panel initialize and registration with runtime depedencies.
int _panel_runtime_init(AdwApplication *app, Panel *panel,
                        GdkMonitor *monitor) {
    _panel_attach_monitor(panel, app, monitor);

    _panel_init_date_time_button(panel);

    g_hash_table_insert(panels, monitor, panel);

    gtk_window_present(GTK_WINDOW(panel->win));
    return 1;
}

// Iterates over the monitors GListModel, creating panels for any new monitors.
// Designed to be a handler for the GListModel(GdkMonitor)::items-changed
// signal.
void _panel_assign_to_monitors(GListModel *monitors, guint position,
                               guint removed, guint added, gpointer gtk_app) {
    uint8_t n = g_list_model_get_n_items(monitors);
    const char *desc = NULL;

    g_debug("bar.c:_bar_assign_to_monitors(): received monitor change event.");
    g_debug("bar.c:_bar_assign_to_monitors(): new number of monitors %d", n);

    for (uint8_t i = 0; i < n; i++) {
        GdkMonitor *mon = g_list_model_get_item(monitors, i);
        if (!gdk_monitor_is_valid(mon)) {
            g_warning(
                "bar.c:_bar_assign_to_monitors() received invalid monitor from "
                "Gdk, moving to next.");
            continue;
        }

        desc = gdk_monitor_get_description(mon);

        if (g_hash_table_contains(panels, mon)) {
            g_debug(
                "bar.c:_bar_assign_to_monitors(): bar already exists for "
                "monitor %s",
                desc);
            continue;
        }

        Panel *panel = g_object_new(PANEL_TYPE, NULL);
        if (!_panel_runtime_init(ADW_APPLICATION(gtk_app), panel, mon)) {
            g_critical(
                "bar.c:_bar_assign_to_monitors(): failed to initialize bar");
            _panel_runtime_dispose(mon, panel);
            return;
        }

        g_signal_connect(mon, "invalidate", G_CALLBACK(_panel_runtime_dispose),
                         panel);

        g_debug(
            "bar.c:_bar_assign_to_monitors(): initialized bar for monitor: %s",
            desc);
    }
}

void panel_activate(AdwApplication *app, gpointer user_data) {
    GdkSeat *seat = gdk_display_get_default_seat(gdk_display_get_default());
    GdkDisplay *display = gdk_seat_get_display(seat);
    GListModel *monitors = gdk_display_get_monitors(display);

    panels = g_hash_table_new(g_direct_hash, g_direct_equal);

    g_debug("bar.c:panel_activate() initializing bars");
    _panel_assign_to_monitors(monitors, 0, 0, 0, app);

    // Connect the callback function to the "items-changed" signal
    // so we'll destroy and recreate bars on monitor changes.
    //
    // The GListModel returned from gdk_display_get_monitors should stay valid
    // until GdkDisplay is closed, at which point we are closing all windows
    // anyway, so don't bother tracking connection ID for later disconnect.
    g_signal_connect(monitors, "items-changed",
                     G_CALLBACK(_panel_assign_to_monitors), app);
}
