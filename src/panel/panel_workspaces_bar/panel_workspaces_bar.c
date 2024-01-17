#include "panel_workspaces_bar.h"

#include <adwaita.h>

#include "../../services/window_manager_service/sway/window_manager_service_sway.h"
#include "../../services/window_manager_service/window_manager_service.h"
#include "../panel.h"

typedef struct _Panel Panel;

typedef struct _PanelWorkspacesBar {
    GObject parent_instance;
    Panel *panel;
    GtkBox *container;
    GtkScrolledWindow *scroll_win;
    GtkBox *list;
    GPtrArray *workspaces;
    guint32 signal_id;
} PanelWorkspacesBar;
G_DEFINE_TYPE(PanelWorkspacesBar, panel_workspaces_bar, G_TYPE_OBJECT);

// button on click handler
static void on_button_clicked(GtkButton *button, PanelWorkspacesBar *self) {
    WMWorkspace *ws = g_object_get_data(G_OBJECT(button), "workspace");
    WMServiceSway *sway = wm_service_sway_get_global();

    g_debug("workspace_bar.c:on_button_clicked() called");
    wm_service_sway_focus_workspace(sway, ws->name);
}

GtkButton *create_workspace_button(WMServiceSway *srv, WMWorkspace *ws,
                                   PanelWorkspacesBar *self) {
    // create button
    GtkWidget *button;
    button = gtk_button_new_with_label(ws->name);

    // add on click handler
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_clicked), self);

    // add panel-button css class
    gtk_widget_add_css_class(button, "panel-button");
    if (ws->focused) {
        gtk_widget_add_css_class(button, "panel-button-toggled");
    }
    if (ws->urgent && !ws->focused) {
        gtk_widget_add_css_class(button, "panel-button-urgent");
    }

    // append workspace pointer to button's data
    // so we can access it later
    g_object_set_data(G_OBJECT(button), "workspace", ws);

    // add button to list
    gtk_box_append(self->list, button);

    if (ws->focused) {
        return GTK_BUTTON(button);
    }
    return NULL;
}

static void on_workspaces_update(WMServiceSway *sway, GPtrArray *workspaces,
                                 PanelWorkspacesBar *self) {
    GtkWidget *child;
    GdkMonitor *mon = panel_get_monitor(self->panel);
    GtkButton *focused = NULL;
    GPtrArray *buttons = g_ptr_array_new();
    GPtrArray *filtered = g_ptr_array_new();
    uint buttons_len = 0;
    uint ws_len = 0;
    g_debug("workspace_bar.c:on_workspaces_update() called");

    if (!workspaces) {
        g_warning("workspaces_bar.c:on_workspaces_update() workspaces is NULL");
        return;
    }
    if (!self) {
        g_warning("workspaces_bar.c:on_workspaces_update() self is NULL");
        return;
    }

    // check if we need to replace the workspaces array.
    if (!self->workspaces) {
        self->workspaces = g_ptr_array_ref(workspaces);
    } else if (self->workspaces != workspaces) {
        g_ptr_array_unref(self->workspaces);
        self->workspaces = g_ptr_array_ref(workspaces);
    }

    if (self->workspaces != workspaces && self->workspaces) {
        g_ptr_array_unref(self->workspaces);
    }

    // filter out workspaces for this monitor
    for (int i = 0; i < workspaces->len; i++) {
        WMWorkspace *ws = g_ptr_array_index(workspaces, i);
        if (g_strcmp0(ws->output, gdk_monitor_get_connector(mon)) == 0) {
            g_ptr_array_add(filtered, ws);
        }
    }
    workspaces = filtered;

    ws_len = workspaces->len;
    g_debug("workspace_bar.c:on_workspaces_update() output [%s] ws_len = %d",
            gdk_monitor_get_connector(mon), ws_len);

    // no workspaces, remove all buttons and return
    if (ws_len == 0) {
        // remove all buttons, removing it from container will unref it as well,
        // and we should be the only refs.
        while ((child = gtk_widget_get_first_child(GTK_WIDGET(self->list)))) {
            gtk_box_remove(self->list, child);
        }
        return;
    }

    for (GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(self->list));
         child; child = gtk_widget_get_next_sibling(child)) {
        g_ptr_array_add(buttons, child);
        buttons_len++;
    }

    // if no buttons to reuse, create one for each and return.
    if (buttons_len == 0) {
        for (int i = 0; i < workspaces->len; i++) {
            GtkButton *button;

            WMWorkspace *ws = g_ptr_array_index(workspaces, i);

            // check if this workspace is for our current GdkConnection
            if (g_strcmp0(ws->output, gdk_monitor_get_connector(mon)) != 0) {
                continue;
            }

            // create button
            g_debug(
                "workspace_bar.c:on_workspaces_update() creating button for "
                "workspace [%s] on output [%s]",
                ws->name, ws->output);
            button = create_workspace_button(sway, ws, self);
            if (button) focused = button;
        }
        return;
    }

    // we have a positive number of buttons and workspaces.
    // reuse as many buttons as we can
    for (int i = 0; i < buttons_len && i < ws_len; i++) {
        GtkButton *button;
        WMWorkspace *ws;

        button = g_ptr_array_index(buttons, i);
        ws = g_ptr_array_index(workspaces, i);

        // reset name label
        gtk_button_set_label(button, ws->name);

        // reset css
        gtk_widget_remove_css_class(GTK_WIDGET(button), "panel-button-urgent");
        gtk_widget_remove_css_class(GTK_WIDGET(button), "panel-button-toggled");

        // set appropriate classes if focused and/or urgent
        if (ws->focused) {
            gtk_widget_add_css_class(GTK_WIDGET(button),
                                     "panel-button-toggled");
            focused = button;
        }
        if (ws->urgent)
            gtk_widget_add_css_class(GTK_WIDGET(button), "panel-button-urgent");

        // overwrite workspace pointer on button's data
        // so we can access it later
        g_object_set_data(G_OBJECT(button), "workspace", ws);
    }

    // if we had more workspaces then buttons we need to add buttons
    if (ws_len > buttons_len) {
        for (int i = buttons_len; i < ws_len; i++) {
            GtkButton *button;

            WMWorkspace *ws = g_ptr_array_index(workspaces, i);

            // check if this workspace is for our current GdkConnection
            if (g_strcmp0(ws->output, gdk_monitor_get_connector(mon)) != 0) {
                continue;
            }

            // create button
            button = create_workspace_button(sway, ws, self);
            if (button) focused = button;
        }
        return;
    }

    // if we had more buttons then workspaces we need to remove some buttons
    if (buttons_len > ws_len) {
        for (int i = ws_len; i < buttons_len; i++) {
            GtkWidget *button = g_ptr_array_index(buttons, i);
            gtk_box_remove(self->list, button);
        }
    }

    // grab focus of button
    if (focused) {
        gtk_widget_grab_focus(GTK_WIDGET(focused));
    }
}

// stub out dispose, finalize, class_init, and init methods
static void panel_workspaces_bar_dispose(GObject *gobject) {
    PanelWorkspacesBar *self = PANEL_WORKSPACES_BAR(gobject);

    g_debug("panel_workspaces_bar.c:panel_workspaces_bar_dispose() called");

    g_debug("workspaces_bar.c:workspaces_bar_dispose() called");

    // disconnect from signals
    WMServiceSway *sway = wm_service_sway_get_global();
    g_signal_handler_disconnect(sway, self->signal_id);

    // release reference to current workspaces array
    g_ptr_array_unref(self->workspaces);

    // Chain-up
    G_OBJECT_CLASS(panel_workspaces_bar_parent_class)->dispose(gobject);
};

static void panel_workspaces_bar_finalize(GObject *gobject) {
    PanelWorkspacesBar *self = PANEL_WORKSPACES_BAR(gobject);

    g_debug("panel_workspaces_bar.c:panel_workspaces_bar_finalize() called");

    // Chain-up
    G_OBJECT_CLASS(panel_workspaces_bar_parent_class)->finalize(gobject);
};

static void panel_workspaces_bar_class_init(PanelWorkspacesBarClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = panel_workspaces_bar_dispose;
    object_class->finalize = panel_workspaces_bar_finalize;
};

static void panel_workpaces_bar_init_layout(PanelWorkspacesBar *self) {
    // get sway window manager service
    WMServiceSway *sway = wm_service_sway_get_global();

    // initialize the container
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->container), "workspaces-bar");

    // initialize scrolled window
    self->scroll_win = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
    gtk_scrolled_window_set_max_content_width(self->scroll_win, 800);
    // set scroll bar never appear policy
    gtk_scrolled_window_set_policy(self->scroll_win, GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_NEVER);
    gtk_scrolled_window_set_propagate_natural_width(self->scroll_win, TRUE);

    // initialize the list
    self->list = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->container), "workspaces-bar-list");

    // add scrolled window to container
    gtk_box_append(self->container, GTK_WIDGET(self->scroll_win));
    // add list to scrolled window
    gtk_scrolled_window_set_child(self->scroll_win, GTK_WIDGET(self->list));

    // get initial list of workspaces
    self->workspaces = wm_service_sway_get_workspaces(sway);

    // wire up to 'workspaces-changed' service
    self->signal_id = g_signal_connect(sway, "workspaces-changed",
                                       G_CALLBACK(on_workspaces_update), self);
}

static void panel_workspaces_bar_init(PanelWorkspacesBar *self) {
    panel_workpaces_bar_init_layout(self);
}

GtkWidget *panel_workspaces_bar_get_widget(PanelWorkspacesBar *self) {
    return GTK_WIDGET(self->container);
}

void panel_workspaces_bar_set_panel(PanelWorkspacesBar *self, Panel *panel) {
    if (!panel)
        g_error(
            "panel_workspaces_bar.c:panel_workspaces_bar_set_panel() panel is "
            "NULL");
    self->panel = panel;
    // create initial buttons
    on_workspaces_update(NULL, self->workspaces, self);
}
