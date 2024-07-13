#include "./workspace_switcher.h"

#include <adwaita.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

#include "../switcher/switcher.h"
#include "./../services/window_manager_service/window_manager_service.h"
#include "./workspace_switcher_workspace_widget.h"
#include "gdk/gdkkeysyms.h"

static WorkspaceSwitcher *global = NULL;

enum signals { signals_n };

enum mode {
    switch_workspace,
    switch_app,
};

typedef struct _WorkspaceSwitcher {
    GObject parent_instance;
    Switcher switcher;
    enum mode mode;

} WorkspaceSwitcher;

static guint workspace_switcher_signals[signals_n] = {0};
G_DEFINE_TYPE(WorkspaceSwitcher, workspace_switcher, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init and init methods.
static void workspace_switcher_dispose(GObject *object) {
    G_OBJECT_CLASS(workspace_switcher_parent_class)->dispose(object);
}

static void workspace_switcher_finalize(GObject *object) {
    G_OBJECT_CLASS(workspace_switcher_parent_class)->finalize(object);
}

static void workspace_switcher_class_init(WorkspaceSwitcherClass *class) {
    GObjectClass *object_class = G_OBJECT_CLASS(class);
    object_class->dispose = workspace_switcher_dispose;
    object_class->finalize = workspace_switcher_finalize;
}

GtkListBoxRow *workspace_switcher_top_choice(WorkspaceSwitcher *self) {
    // always select first visible entry
    GtkListBoxRow *child = GTK_LIST_BOX_ROW(
        gtk_widget_get_first_child(GTK_WIDGET(SWITCHER(self).list)));
    while (child) {
        if (gtk_widget_get_child_visible(GTK_WIDGET(child))) {
            return child;
            break;
        }
        child =
            GTK_LIST_BOX_ROW(gtk_widget_get_next_sibling(GTK_WIDGET(child)));
    }
    return NULL;
}

GtkListBoxRow *workspace_switcher_last_choice(WorkspaceSwitcher *self) {
    // always select first visible entry
    GtkListBoxRow *found = NULL;
    GtkListBoxRow *child = GTK_LIST_BOX_ROW(
        gtk_widget_get_first_child(GTK_WIDGET(SWITCHER(self).list)));
    while (child) {
        if (gtk_widget_get_child_visible(GTK_WIDGET(child))) {
            found = child;
        }
        child =
            GTK_LIST_BOX_ROW(gtk_widget_get_next_sibling(GTK_WIDGET(child)));
    }
    return found;
}

static void on_search_changed(GtkSearchEntry *entry, WorkspaceSwitcher *self);

static void workspace_switcher_clear_search(WorkspaceSwitcher *self) {
    // block search entry change signal
    g_signal_handlers_block_by_func(SWITCHER(self).search_entry,
                                    on_search_changed, self);
    // clear search entry
    gtk_editable_set_text(GTK_EDITABLE(SWITCHER(self).search_entry), "");

    // unblock signal
    g_signal_handlers_unblock_by_func(SWITCHER(self).search_entry,
                                      on_search_changed, self);
}

static void on_window_destroy(GtkWindow *win, WorkspaceSwitcher *self) {}

static gboolean filter_func(GtkListBoxRow *row, GtkSearchEntry *entry) {
    GtkWidget *widget = gtk_list_box_row_get_child(row);

    WMWorkspace *ws = g_object_get_data(G_OBJECT(widget), "workspace");

    const char *search_text = gtk_editable_get_text(GTK_EDITABLE(entry));
    gboolean match = g_str_match_string(search_text, ws->name, true);
    return match;
}

static void on_search_next_match(GtkSearchEntry *entry,
                                 WorkspaceSwitcher *self) {
    g_debug("workspace_switcher.c:on_search_next_match() called.");

    GtkListBoxRow *start = gtk_list_box_get_selected_row(SWITCHER(self).list);
    if (!start) start = workspace_switcher_top_choice(self);

    if (!start) return;

    GtkListBoxRow *next_row = gtk_list_box_get_row_at_index(
        SWITCHER(self).list, gtk_list_box_row_get_index(start) + 1);
    if (!next_row) next_row = workspace_switcher_top_choice(self);

    gtk_list_box_select_row(SWITCHER(self).list, next_row);
    gtk_widget_grab_focus(GTK_WIDGET(next_row));
}

static void on_search_previous_match(GtkSearchEntry *entry,
                                     WorkspaceSwitcher *self) {
    g_debug("workspace_switcher.c:on_search_previous_match() called.");

    GtkListBoxRow *start = gtk_list_box_get_selected_row(SWITCHER(self).list);
    if (!start) start = workspace_switcher_top_choice(self);

    GtkListBoxRow *next_row = gtk_list_box_get_row_at_index(
        SWITCHER(self).list, gtk_list_box_row_get_index(start) - 1);
    if (!next_row) next_row = workspace_switcher_last_choice(self);

    gtk_list_box_select_row(SWITCHER(self).list, next_row);
    gtk_widget_grab_focus(GTK_WIDGET(next_row));
}

static void on_search_activated_app_mode(GtkSearchEntry *entry,
                                         WorkspaceSwitcher *self) {
    if (!workspace_switcher_top_choice(self)) {
        const gchar *search_text = gtk_editable_get_text(GTK_EDITABLE(entry));
        WMWorkspace ws = {.num = -1, .name = (void *)search_text};

        WindowManager *wm = window_manager_service_get_global();
        wm->current_app_to_workspace(wm, &ws);

        workspace_switcher_hide(self);
        return;
    }

    GtkListBoxRow *selected =
        gtk_list_box_get_selected_row(SWITCHER(self).list);

    GtkWidget *widget = gtk_list_box_row_get_child(selected);
    if (!widget) return;

    WMWorkspace *ws = g_object_get_data(G_OBJECT(widget), "workspace");
    if (!ws) {
        // should not happen...
        g_error("Workspace not found.");
    }

    // switch to workspace
    WindowManager *wm = window_manager_service_get_global();
    wm->current_app_to_workspace(wm, ws);

    // hide widget
    workspace_switcher_hide(self);
}

static void activate_with_current_search_entry(GtkSearchEntry *entry,
                                               WorkspaceSwitcher *self) {
    const gchar *search_text = gtk_editable_get_text(GTK_EDITABLE(entry));
    WMWorkspace ws = {.num = -1, .name = (void *)search_text};

    WindowManager *wm = window_manager_service_get_global();
    wm->focus_workspace(wm, &ws);

    workspace_switcher_hide(self);
    return;
}

static void on_search_activated(GtkSearchEntry *entry,
                                WorkspaceSwitcher *self) {
    if (self->mode == switch_app) {
        on_search_activated_app_mode(entry, self);
        return;
    }

    if (!workspace_switcher_top_choice(self)) {
        activate_with_current_search_entry(entry, self);
        return;
    }

    GtkListBoxRow *selected =
        gtk_list_box_get_selected_row(SWITCHER(self).list);

    GtkWidget *widget = gtk_list_box_row_get_child(selected);
    if (!widget) return;

    WMWorkspace *ws = g_object_get_data(G_OBJECT(widget), "workspace");
    if (!ws) {
        // should not happen...
        g_error("Workspace not found.");
    }

    // switch to workspace
    WindowManager *wm = window_manager_service_get_global();
    wm->focus_workspace(wm, ws);

    // hide widget
    workspace_switcher_hide(self);
}

static void on_search_stop(GtkSearchEntry *entry, WorkspaceSwitcher *self) {
    if (entry) {
        const char *search_text = gtk_editable_get_text(GTK_EDITABLE(entry));
        if (strlen(search_text) == 0) {
            gtk_widget_set_visible(GTK_WIDGET(SWITCHER(self).win), false);
            gtk_list_box_invalidate_filter(SWITCHER(self).list);
            return;
        }
    }

    workspace_switcher_clear_search(self);
    // invalidate filter
    gtk_list_box_invalidate_filter(SWITCHER(self).list);
}

static void on_search_changed(GtkSearchEntry *entry, WorkspaceSwitcher *self) {
    const char *search_text = gtk_editable_get_text(GTK_EDITABLE(entry));
    // if search string is empty call on_search_stop
    if (strlen(search_text) == 0) {
        // invalidate filter
        gtk_list_box_invalidate_filter(SWITCHER(self).list);
        return;
    }

    // invlidate filter
    gtk_list_box_invalidate_filter(SWITCHER(self).list);

    // apply new filter
    gtk_list_box_set_filter_func(
        SWITCHER(self).list, (GtkListBoxFilterFunc)filter_func, entry, NULL);

    GtkListBoxRow *top_choice = workspace_switcher_top_choice(self);
    if (top_choice) {
        gtk_list_box_select_row(SWITCHER(self).list, top_choice);
    }
}

static void on_workspaces_changed(void *data, GPtrArray *workspaces) {
    WorkspaceSwitcher *self = (WorkspaceSwitcher *)data;

    g_debug("workspace_switcher.c:on_workspaces_changed() called.");
    if (!workspaces) return;

    gtk_list_box_remove_all(SWITCHER(self).list);

    for (guint i = 0; i < workspaces->len; i++) {
        WMWorkspace *workspace = g_ptr_array_index(workspaces, i);

        WorkspaceSwitcherWorkspaceWidget *widget =
            g_object_new(WORKSPACE_SWITCHER_WORKSPACE_WIDGET_TYPE, NULL);
        workspace_switcher_workspace_widget_set_workspace_name(widget,
                                                               workspace->name);

        g_object_set_data(
            G_OBJECT(workspace_switcher_workspace_widget_get_widget(widget)),
            "workspace", workspace);
        gtk_list_box_append(
            SWITCHER(self).list,
            workspace_switcher_workspace_widget_get_widget(widget));
    }
}

static void on_row_activated(GtkListBox *box, GtkListBoxRow *row,
                             WorkspaceSwitcher *self) {
    GtkWidget *widget = gtk_list_box_row_get_child(row);
    if (!widget) return;

    WMWorkspace *ws = g_object_get_data(G_OBJECT(widget), "workspace");
    if (!ws) {
        // should not happen...
        g_error("Workspace not found.");
    }

    WindowManager *wm = window_manager_service_get_global();
    if (self->mode == switch_app)
        wm->current_app_to_workspace(wm, ws);
    else
        wm->focus_workspace(wm, ws);

    // hide widget
    workspace_switcher_hide(self);
}

static gboolean key_pressed(GtkEventControllerKey *controller, guint keyval,
                            guint keycode, GdkModifierType state,
                            WorkspaceSwitcher *self) {
    if (keyval == GDK_KEY_Down) {
        on_search_next_match(NULL, self);
        return true;
    }

    if (keyval == GDK_KEY_Up) {
        on_search_previous_match(NULL, self);
        return true;
    }

    if (keyval == GDK_KEY_Tab) {
        on_search_next_match(NULL, self);
        return true;
    }

    if (keyval == GDK_KEY_ISO_Left_Tab && (state & GDK_SHIFT_MASK)) {
        on_search_previous_match(NULL, self);
        return true;
    }

    if (keyval == GDK_KEY_n && (state & GDK_CONTROL_MASK)) {
        on_search_next_match(NULL, self);
        return true;
    }

    if (keyval == GDK_KEY_p && (state & GDK_CONTROL_MASK)) {
        on_search_previous_match(NULL, self);
        return true;
    }

    if (keyval == GDK_KEY_p && (state & GDK_CONTROL_MASK)) {
        on_search_previous_match(NULL, self);
        return true;
    }

    // if the user holds ctrl and presses enter, ignore the search result and
    // activate/create the workspace in the search entry.
    //
    // this allows the search to be fuzzy, but still provides a way for a user
    // to create a workspace if the desired name overlaps with an existing one.
    if (keyval == GDK_KEY_Return && (state & GDK_CONTROL_MASK)) {
        activate_with_current_search_entry(SWITCHER(self).search_entry, self);
        return true;
    }

    return false;
}

static void workspace_switcher_init_layout(WorkspaceSwitcher *self) {
    switcher_init(&self->switcher, true);

    // wire into search-changed signal for GtkSearchEntry
    g_signal_connect(SWITCHER(self).search_entry, "search-changed",
                     G_CALLBACK(on_search_changed), self);

    // wire into stop-search signal for GtkSearchEntry
    g_signal_connect(SWITCHER(self).search_entry, "stop-search",
                     G_CALLBACK(on_search_stop), self);

    // wire into 'activate' signal
    g_signal_connect(SWITCHER(self).search_entry, "activate",
                     G_CALLBACK(on_search_activated), self);

    // wire into 'next-match' signal
    g_signal_connect(SWITCHER(self).search_entry, "next-match",
                     G_CALLBACK(on_search_next_match), self);

    // wire into 'previous-match' signal
    g_signal_connect(SWITCHER(self).search_entry, "previous-match",
                     G_CALLBACK(on_search_previous_match), self);

    // get listings of workspaces
    WindowManager *wm = window_manager_service_get_global();
    GPtrArray *workspaces = wm->get_workspaces(wm);
    on_workspaces_changed(self, workspaces);

    wm->register_on_workspaces_changed(wm, on_workspaces_changed, self);

    // wire into GtkListBox's activated
    g_signal_connect(SWITCHER(self).list, "row-activated",
                     G_CALLBACK(on_row_activated), self);

    // hook up keymap
    g_signal_connect(SWITCHER(self).key_controller, "key-pressed",
                     G_CALLBACK(key_pressed), self);
}

static void workspace_switcher_init(WorkspaceSwitcher *self) {
    workspace_switcher_init_layout(self);
}

WorkspaceSwitcher *workspace_switcher_get_global() { return global; }

void workspace_switcher_activate(AdwApplication *app, gpointer user_data) {
    if (global == NULL) {
        global = g_object_new(WORKSPACE_SWITCHER_TYPE, NULL);
    }
}

void workspace_switcher_show(WorkspaceSwitcher *self) {
    g_debug("workspace_switcher.c:workspace_switcher_show() called.");

    GtkListBoxRow *row = gtk_list_box_get_row_at_index(SWITCHER(self).list, 0);
    if (row) gtk_list_box_select_row(SWITCHER(self).list, row);

    // grab search entry focus
    gtk_widget_grab_focus(GTK_WIDGET(SWITCHER(self).search_entry));

    gtk_window_present(GTK_WINDOW(SWITCHER(self).win));
}

void workspace_switcher_hide(WorkspaceSwitcher *self) {
    g_debug("workspace_switcher.c:workspace_switcher_hide() called.");

    self->mode = switch_workspace;

    on_search_stop(NULL, self);

    gtk_widget_set_visible(GTK_WIDGET(SWITCHER(self).win), false);
}

void workspace_switcher_toggle(WorkspaceSwitcher *self) {
    g_debug("workspace_switcher.c:workspace_switcher_toggle() called.");
    if (gtk_widget_get_visible(GTK_WIDGET(SWITCHER(self).win))) {
        workspace_switcher_hide(self);
    } else {
        workspace_switcher_show(self);
    }
}

void workspace_switcher_show_app_mode(WorkspaceSwitcher *self) {
    g_debug("workspace_switcher.c:workspace_switcher_show() called.");

    GtkListBoxRow *row = gtk_list_box_get_row_at_index(SWITCHER(self).list, 0);
    if (row) gtk_list_box_select_row(SWITCHER(self).list, row);

    self->mode = switch_app;

    // grab search entry focus
    gtk_widget_grab_focus(GTK_WIDGET(SWITCHER(self).search_entry));

    gtk_window_present(GTK_WINDOW(SWITCHER(self).win));
}

void workspace_switcher_toggle_app_mode(WorkspaceSwitcher *self) {
    g_debug("workspace_switcher.c:workspace_switcher_toggle() called.");
    if (gtk_widget_get_visible(GTK_WIDGET(SWITCHER(self).win))) {
        workspace_switcher_hide(self);
    } else {
        workspace_switcher_show_app_mode(self);
    }
}
