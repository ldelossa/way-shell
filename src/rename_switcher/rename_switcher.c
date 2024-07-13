#include "./rename_switcher.h"

#include <adwaita.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

#include "../switcher/switcher.h"
#include "./../services/window_manager_service/window_manager_service.h"
#include "gdk/gdkkeysyms.h"

static RenameSwitcher *global = NULL;

enum signals { signals_n };

enum mode {
    switch_rename,
    switch_app,
};

typedef struct _RenameSwitcher {
    GObject parent_instance;
    Switcher switcher;

} RenameSwitcher;

static guint rename_switcher_signals[signals_n] = {0};
G_DEFINE_TYPE(RenameSwitcher, rename_switcher, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init and init methods.
static void rename_switcher_dispose(GObject *object) {
    G_OBJECT_CLASS(rename_switcher_parent_class)->dispose(object);
}

static void rename_switcher_finalize(GObject *object) {
    G_OBJECT_CLASS(rename_switcher_parent_class)->finalize(object);
}

static void rename_switcher_class_init(RenameSwitcherClass *class) {
    GObjectClass *object_class = G_OBJECT_CLASS(class);
    object_class->dispose = rename_switcher_dispose;
    object_class->finalize = rename_switcher_finalize;
}

static void rename_switcher_clear_search(RenameSwitcher *self) {
    // clear search entry
    gtk_editable_set_text(GTK_EDITABLE(SWITCHER(self).search_entry), "");
}


static void on_search_activated(GtkSearchEntry *entry, RenameSwitcher *self) {
    // get search entry text
    const gchar *search_text = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (strlen(search_text) == 0) return;

    // switch to rename
    WindowManager *wm = window_manager_service_get_global();
    wm->rename_workspace(wm, search_text);

    // hide widget
    rename_switcher_hide(self);
}

static void on_search_stop(GtkSearchEntry *entry, RenameSwitcher *self) {
    if (entry) {
        const char *search_text = gtk_editable_get_text(GTK_EDITABLE(entry));
        if (strlen(search_text) == 0) {
            gtk_widget_set_visible(GTK_WIDGET(SWITCHER(self).win), false);
            gtk_list_box_invalidate_filter(SWITCHER(self).list);
            return;
        }
    }
    rename_switcher_clear_search(self);
}

static gboolean key_pressed(GtkEventControllerKey *controller, guint keyval,
                            guint keycode, GdkModifierType state,
                            RenameSwitcher *self) {
    return false;
}

static void rename_switcher_init_layout(RenameSwitcher *self) {
    switcher_init(&self->switcher, false);

    // wire into stop-search signal for GtkSearchEntry
    g_signal_connect(SWITCHER(self).search_entry, "stop-search",
                     G_CALLBACK(on_search_stop), self);

    // wire into 'activate' signal
    g_signal_connect(SWITCHER(self).search_entry, "activate",
                     G_CALLBACK(on_search_activated), self);

    // hook up keymap
    g_signal_connect(SWITCHER(self).key_controller, "key-pressed",
                     G_CALLBACK(key_pressed), self);
}

static void rename_switcher_init(RenameSwitcher *self) {
    rename_switcher_init_layout(self);
}

RenameSwitcher *rename_switcher_get_global() { return global; }

void rename_switcher_activate(AdwApplication *app, gpointer user_data) {
    if (global == NULL) {
        global = g_object_new(RENAME_SWITCHER_TYPE, NULL);
    }
}

void rename_switcher_show(RenameSwitcher *self) {
    g_debug("rename_switcher.c:rename_switcher_show() called.");

    // grab search entry focus
    gtk_widget_grab_focus(GTK_WIDGET(SWITCHER(self).search_entry));

    gtk_window_present(GTK_WINDOW(SWITCHER(self).win));
}

void rename_switcher_hide(RenameSwitcher *self) {
    g_debug("rename_switcher.c:rename_switcher_hide() called.");

    on_search_stop(NULL, self);

    gtk_widget_set_visible(GTK_WIDGET(SWITCHER(self).win), false);
}

void rename_switcher_toggle(RenameSwitcher *self) {
    g_debug("rename_switcher.c:rename_switcher_toggle() called.");
    if (gtk_widget_get_visible(GTK_WIDGET(SWITCHER(self).win))) {
        rename_switcher_hide(self);
    } else {
        rename_switcher_show(self);
    }
}
