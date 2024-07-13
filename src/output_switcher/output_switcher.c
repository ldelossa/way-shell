#include "./output_switcher.h"

#include <adwaita.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

#include "../switcher/switcher.h"
#include "./../services/window_manager_service/window_manager_service.h"
#include "./output_switcher_output_widget.h"
#include "gdk/gdkkeysyms.h"

static OutputSwitcher *global = NULL;

enum signals { signals_n };

typedef struct _OutputSwitcher {
    GObject parent_instance;
    Switcher switcher;
} OutputSwitcher;

static guint output_switcher_signals[signals_n] = {0};
G_DEFINE_TYPE(OutputSwitcher, output_switcher, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init and init methods.
static void output_switcher_dispose(GObject *object) {
    G_OBJECT_CLASS(output_switcher_parent_class)->dispose(object);
}

static void output_switcher_finalize(GObject *object) {
    G_OBJECT_CLASS(output_switcher_parent_class)->finalize(object);
}

static void output_switcher_class_init(OutputSwitcherClass *class) {
    GObjectClass *object_class = G_OBJECT_CLASS(class);
    object_class->dispose = output_switcher_dispose;
    object_class->finalize = output_switcher_finalize;
}

static void on_search_changed(GtkSearchEntry *entry, OutputSwitcher *self);

static void output_switcher_clear_search(OutputSwitcher *self) {
    // block search entry change signal
    g_signal_handlers_block_by_func(SWITCHER(self).search_entry,
                                    on_search_changed, self);
    // clear search entry
    gtk_editable_set_text(GTK_EDITABLE(SWITCHER(self).search_entry), "");

    // unblock signal
    g_signal_handlers_unblock_by_func(SWITCHER(self).search_entry,
                                      on_search_changed, self);
}

static void on_window_destroy(GtkWindow *win, OutputSwitcher *self) {}

static gboolean filter_func(GtkListBoxRow *row, GtkSearchEntry *entry) {
    GtkWidget *widget = gtk_list_box_row_get_child(row);

    WMOutput *ws = g_object_get_data(G_OBJECT(widget), "output");

    const char *search_text = gtk_editable_get_text(GTK_EDITABLE(entry));
    gboolean match = g_str_match_string(search_text, ws->name, true);
    return match;
}

static void on_search_next_match(GtkSearchEntry *entry, OutputSwitcher *self) {
    g_debug("output_switcher.c:on_search_next_match() called.");

    GtkListBoxRow *start = gtk_list_box_get_selected_row(SWITCHER(self).list);
    if (!start) start = switcher_top_choice(&SWITCHER(self));

    if (!start) return;

    GtkListBoxRow *next_row = gtk_list_box_get_row_at_index(
        SWITCHER(self).list, gtk_list_box_row_get_index(start) + 1);
    if (!next_row) next_row = switcher_top_choice(&SWITCHER(self));

    gtk_list_box_select_row(SWITCHER(self).list, next_row);
    gtk_widget_grab_focus(GTK_WIDGET(next_row));
}

static void on_search_previous_match(GtkSearchEntry *entry,
                                     OutputSwitcher *self) {
    g_debug("output_switcher.c:on_search_previous_match() called.");

    GtkListBoxRow *start = gtk_list_box_get_selected_row(SWITCHER(self).list);
    if (!start) start = switcher_top_choice(&SWITCHER(self));

    GtkListBoxRow *next_row = gtk_list_box_get_row_at_index(
        SWITCHER(self).list, gtk_list_box_row_get_index(start) - 1);
    if (!next_row) next_row = switcher_last_choice(&SWITCHER(self));

    gtk_list_box_select_row(SWITCHER(self).list, next_row);
    gtk_widget_grab_focus(GTK_WIDGET(next_row));
}

static void on_search_activated(GtkSearchEntry *entry, OutputSwitcher *self) {
    if (!switcher_top_choice(&SWITCHER(self))) {
        const gchar *search_text = gtk_editable_get_text(GTK_EDITABLE(entry));
        WMOutput ws = {.name = (void *)search_text};

        WindowManager *wm = window_manager_service_get_global();
        wm->current_ws_to_output(wm, &ws);

        output_switcher_hide(self);
        return;
    }

    GtkListBoxRow *selected =
        gtk_list_box_get_selected_row(SWITCHER(self).list);

    GtkWidget *widget = gtk_list_box_row_get_child(selected);
    if (!widget) return;

    OutputSwitcherOutputWidget *output =
        g_object_get_data(G_OBJECT(widget), "output-widget");

    WMOutput o = {
        .name = output_switcher_output_widget_get_output_name(output),
    };

    // switch to output
    WindowManager *wm = window_manager_service_get_global();
    wm->current_ws_to_output(wm, &o);

    // hide widget
    output_switcher_hide(self);
}

static void on_search_stop(GtkSearchEntry *entry, OutputSwitcher *self) {
    if (entry) {
        const char *search_text = gtk_editable_get_text(GTK_EDITABLE(entry));
        if (strlen(search_text) == 0) {
            gtk_widget_set_visible(GTK_WIDGET(SWITCHER(self).win), false);
            gtk_list_box_invalidate_filter(SWITCHER(self).list);
            return;
        }
    }

    output_switcher_clear_search(self);
    // invalidate filter
    gtk_list_box_invalidate_filter(SWITCHER(self).list);
}

static void on_search_changed(GtkSearchEntry *entry, OutputSwitcher *self) {
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

    GtkListBoxRow *top_choice = switcher_top_choice(&SWITCHER(self));
    if (top_choice) {
        gtk_list_box_select_row(SWITCHER(self).list, top_choice);
    }
}

static void on_outputs_changed(void *data, GPtrArray *outputs) {
    g_debug("output_switcher.c:on_outputs_changed() called.");
    if (!outputs) return;

    OutputSwitcher *self = data;

    gtk_list_box_remove_all(SWITCHER(self).list);

    for (guint i = 0; i < outputs->len; i++) {
        WMOutput *output = g_ptr_array_index(outputs, i);

        OutputSwitcherOutputWidget *widget =
            g_object_new(OUTPUT_SWITCHER_OUTPUT_WIDGET_TYPE, NULL);
        output_switcher_output_widget_set_output_name(widget, output->name);

        g_object_set_data(
            G_OBJECT(output_switcher_output_widget_get_widget(widget)),
            "output", output);
        gtk_list_box_append(SWITCHER(self).list,
                            output_switcher_output_widget_get_widget(widget));
    }
}

static void on_row_activated(GtkListBox *box, GtkListBoxRow *row,
                             OutputSwitcher *self) {
    GtkWidget *widget = gtk_list_box_row_get_child(row);
    if (!widget) return;

    OutputSwitcherOutputWidget *output =
        g_object_get_data(G_OBJECT(widget), "output-widget");

    WMOutput o = {
        .name = output_switcher_output_widget_get_output_name(output),
    };

    // switch to output
    WindowManager *wm = window_manager_service_get_global();
    wm->current_ws_to_output(wm, &o);

    // hide widget
    output_switcher_hide(self);
}

static gboolean key_pressed(GtkEventControllerKey *controller, guint keyval,
                            guint keycode, GdkModifierType state,
                            OutputSwitcher *self) {
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

    return false;
}

static void output_switcher_init_layout(OutputSwitcher *self) {
    switcher_init(&SWITCHER(self), true);

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

    // get listings of outputs
    WindowManager *wm = window_manager_service_get_global();
    GPtrArray *outputs = wm->get_outputs(wm);
    on_outputs_changed(self, outputs);

    // wire into outputs changed events
    wm->register_on_outputs_changed(wm, on_outputs_changed, self);

    // wire into GtkListBox's activated
    g_signal_connect(SWITCHER(self).list, "row-activated",
                     G_CALLBACK(on_row_activated), self);

    // wire up keymap
    g_signal_connect(SWITCHER(self).key_controller, "key-pressed",
                     G_CALLBACK(key_pressed), self);
}

static void output_switcher_init(OutputSwitcher *self) {
    output_switcher_init_layout(self);
}

OutputSwitcher *output_switcher_get_global() { return global; }

void output_switcher_activate(AdwApplication *app, gpointer user_data) {
    if (global == NULL) {
        global = g_object_new(OUTPUT_SWITCHER_TYPE, NULL);
    }
}

void output_switcher_show(OutputSwitcher *self) {
    g_debug("output_switcher.c:output_switcher_show() called.");

    GtkListBoxRow *row = gtk_list_box_get_row_at_index(SWITCHER(self).list, 0);
    if (row) gtk_list_box_select_row(SWITCHER(self).list, row);

    // grab search entry focus
    gtk_widget_grab_focus(GTK_WIDGET(SWITCHER(self).search_entry));

    gtk_window_present(GTK_WINDOW(SWITCHER(self).win));
}

void output_switcher_hide(OutputSwitcher *self) {
    g_debug("output_switcher.c:output_switcher_hide() called.");

    on_search_stop(NULL, self);

    gtk_widget_set_visible(GTK_WIDGET(SWITCHER(self).win), false);
}

void output_switcher_toggle(OutputSwitcher *self) {
    g_debug("output_switcher.c:output_switcher_toggle() called.");
    if (gtk_widget_get_visible(GTK_WIDGET(SWITCHER(self).win))) {
        output_switcher_hide(self);
    } else {
        output_switcher_show(self);
    }
}
