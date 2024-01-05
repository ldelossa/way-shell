#include "panel.h"

#include <adwaita.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

#include "panel_clock.h"
#include "panel_mediator.h"
#include "quick_settings_button.h"
#include "workspaces_bar/workspaces_bar.h"
#include "message_tray/message_tray.h"
#include "message_tray/message_tray_mediator.h"

// Provides a mapping between monitors and valid Panels.
// Keys are GdkMonitor pointers while values are *Panel pointers.
//
// The Panel subsystem reconciles GdkMonitor events such that changes to
// monitors (new/removed) also destroy/create Panels.
static GHashTable *panels = NULL;

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
    WorkSpacesBar *ws_bar;
    PanelClock *clock;
    QuickSettingsButton *qs_button;
};
G_DEFINE_TYPE(Panel, panel, G_TYPE_OBJECT)

static void panel_dispose(GObject *gobject) {
    Panel *self = PANEL_PANEL(gobject);

    // will unref all children widgets.
    gtk_window_destroy((GTK_WINDOW(self->win)));

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

    self->container = GTK_CENTER_BOX(gtk_center_box_new());
    gtk_widget_set_name(GTK_WIDGET(self->container), "panel");
    self->left = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    self->center = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    self->right = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

    adw_window_set_content(ADW_WINDOW(self->win), NULL);
    gtk_center_box_set_start_widget(self->container, GTK_WIDGET(self->left));
    gtk_center_box_set_center_widget(self->container, GTK_WIDGET(self->center));
    gtk_center_box_set_end_widget(self->container, GTK_WIDGET(self->right));

    adw_window_set_content(ADW_WINDOW(self->win), GTK_WIDGET(self->container));
};

// Performs our runtime dispose before unref'ing the Panel.
// Designed as a handler for GdkMonior::invalidate signal.
static void panel_runtime_dispose(GdkMonitor *mon, Panel *panel) {
    const char *desc = gdk_monitor_get_description(mon);
    g_debug(
        "panel.c:panel_runtime_dispose(): received monitor invalidation event "
        "for: %s",
        desc);

    // request message tray to close
    MessageTrayMediator *mtm  = message_tray_get_global_mediator();
    message_tray_mediator_req_close(mtm);

    g_hash_table_remove(panels, mon);
    workspaces_bar_free(panel->ws_bar);
    g_object_unref(panel->monitor);
    g_object_unref(panel->clock);
    g_object_unref(panel->qs_button);
    g_object_unref(panel);

}

static void panel_init_attach_workspaces_bar(Panel *panel) {
    panel->ws_bar = workspaces_bar_new(panel);
    gtk_box_append(panel->left,
                   GTK_WIDGET(workspaces_bar_get_widget(panel->ws_bar)));
}

static void panel_init_attach_clock(Panel *panel) {
    panel->clock = g_object_new(PANEL_CLOCK_TYPE, NULL);
    gtk_box_append(panel->center, panel_clock_get_widget(panel->clock));
    gtk_widget_set_halign(GTK_WIDGET(panel->center), GTK_ALIGN_CENTER);
    panel->clock = panel->clock;
    panel_clock_set_panel(panel->clock, panel);
}

static void panel_init_attach_qs_button(Panel *panel) {
    QuickSettingsButton *qs_button =
        g_object_new(QUICK_SETTINGS_BUTTON_TYPE, NULL);
    quick_settings_button_set_panel(qs_button, panel);
    gtk_box_append(panel->right,
                   GTK_WIDGET(quick_settings_button_get_widget(qs_button)));
    panel->qs_button = qs_button;
}

// Attach the Panel to the GtkApplication and fix it to the provided GdkMonitor
static void panel_init_attach_monitor(Panel *panel, AdwApplication *app,
                                      GdkMonitor *monitor) {
    panel->monitor = monitor;
    g_object_ref(panel->monitor);

    gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(panel->win));
    gtk_layer_init_for_window(GTK_WINDOW(panel->win));
    gtk_layer_set_layer((GTK_WINDOW(panel->win)), GTK_LAYER_SHELL_LAYER_TOP);
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
static int panel_runtime_init(AdwApplication *app, Panel *panel,
                              GdkMonitor *monitor) {
    panel_init_attach_monitor(panel, app, monitor);
    panel_init_attach_workspaces_bar(panel);
    panel_init_attach_clock(panel);
    panel_init_attach_qs_button(panel);
    g_hash_table_insert(panels, monitor, panel);
    gtk_window_present(GTK_WINDOW(panel->win));
    return 1;
}

// Iterates over the monitors GListModel, creating panels for any new monitors.
// Designed to be a handler for the GListModel(GdkMonitor)::items-changed
// signal.
static void panel_on_monitor_change(GListModel *monitors, guint position,
                                    guint removed, guint added,
                                    gpointer gtk_app) {
    uint8_t n = g_list_model_get_n_items(monitors);
    const char *desc = NULL;
    const char *model = NULL;
    const char *connector = NULL;

    g_debug(
        "panel.c:panel_on_monitor_change(): received monitor change event.");
    g_debug("panel.c:panel_on_monitor_change(): new number of monitors %d", n);

    for (uint8_t i = 0; i < n; i++) {
        GdkMonitor *mon = g_list_model_get_item(monitors, i);
        if (!gdk_monitor_is_valid(mon)) {
            g_warning(
                "panel.c:panel_on_monitor_change() received invalid monitor "
                "from "
                "Gdk, moving to next.");
            continue;
        }

        desc = gdk_monitor_get_description(mon);
        model = gdk_monitor_get_model(mon);
        connector = gdk_monitor_get_connector(mon);

        if (g_hash_table_contains(panels, mon)) {
            g_debug(
                "panel.c:panel_on_monitor_change(): bar already exists for "
                "monitor [%s] [%s] [%s]",
                desc, model, connector);
            continue;
        }

        Panel *panel = g_object_new(PANEL_TYPE, NULL);
        if (!panel_runtime_init(ADW_APPLICATION(gtk_app), panel, mon)) {
            g_critical(
                "panel.c:panel_on_monitor_change(): failed to initialize bar");
            panel_runtime_dispose(mon, panel);
            return;
        }

        g_signal_connect(mon, "invalidate", G_CALLBACK(panel_runtime_dispose),
                         panel);

        g_debug(
            "panel.c:panel_on_monitor_change(): initialized bar for monitor: "
            "[%s] [%s] [%s]",
            desc, model, connector);
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
    quick_settings_button_set_toggled(panel->qs_button, TRUE);
}

void panel_on_qs_hidden(Panel *panel) {
    g_debug("panel.c:panel_on_qs_hidden() called.");
    quick_settings_button_set_toggled(panel->qs_button, FALSE);
}

void panel_activate(AdwApplication *app, gpointer user_data) {
    mediator = g_object_new(PANEL_MEDIATOR_TYPE, NULL);
    panels = g_hash_table_new(g_direct_hash, g_direct_equal);

    g_debug(
        "panel.c:panel_activate() initializing bars with synthetic monitor "
        "event.");

    GdkSeat *seat = gdk_display_get_default_seat(gdk_display_get_default());
    GdkDisplay *display = gdk_seat_get_display(seat);
    GListModel *monitors = gdk_display_get_monitors(display);

    panel_on_monitor_change(monitors, 0, 0, 0, app);

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
