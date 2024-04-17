#include "./app_switcher.h"

#include <adwaita.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

#include "./../services/wayland_service/wayland_service.h"
#include "./app_switcher_app_widget.h"
#include "gdk/gdkkeysyms.h"

static AppSwitcher *global = NULL;

enum signals { signals_n };

typedef struct _AppSwitcher {
    GObject parent_instance;
    AdwWindow *win;
    GPtrArray *app_widgets;
    GtkBox *app_widget_list;
    GtkEventController *key_controller;
    // tracks which widget should be set as focused.
    gint focused_index;
    // tracks the most recently activated widget.
    gint last_activated_index;
    // tracks the penultimate most recently activated widget.
    gint previously_activated_index;
} AppSwitcher;

static guint app_switcher_signals[signals_n] = {0};
G_DEFINE_TYPE(AppSwitcher, app_switcher, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init and init methods.
static void app_switcher_dispose(GObject *object) {
    G_OBJECT_CLASS(app_switcher_parent_class)->dispose(object);
}

static void app_switcher_finalize(GObject *object) {
    G_OBJECT_CLASS(app_switcher_parent_class)->finalize(object);
}

static void app_switcher_class_init(AppSwitcherClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = app_switcher_dispose;
    object_class->finalize = app_switcher_finalize;
}

static void app_switcher_init_layout(AppSwitcher *self);

static void on_window_destroy(GtkWindow *win, AppSwitcher *self) {
    g_debug("activities.c:on_window_destroy called");

    // empty app_widgets array
    g_ptr_array_set_size(self->app_widgets, 0);

    WaylandService *wayland = wayland_service_get_global();
    wayland_wlr_shortcuts_inhibitor_destroy(wayland);

    gtk_widget_remove_controller(GTK_WIDGET(self->win), self->key_controller);

    app_switcher_init_layout(self);
}

gint app_switcher_find_widget_by_app_id(AppSwitcher *self, gchar *app_id,
                                        AppSwitcherAppWidget **widget) {
    AppSwitcherAppWidget *found = NULL;
    WaylandWLRForeignTopLevel *toplevel = NULL;
    for (gint i = 0; i < self->app_widgets->len; i++) {
        found = g_ptr_array_index(self->app_widgets, i);
        gchar *found_app_id = app_switcher_app_widget_get_app_id(found);
        if (g_strcmp0(app_id, found_app_id) == 0) {
            *widget = found;
            return i;
        }
    }
    return -1;
}

static void app_switcher_sync_display_widgets(AppSwitcher *self) {
    g_ptr_array_set_size(self->app_widgets, 0);

    AppSwitcherAppWidget *prev = NULL;
    if (self->previously_activated_index >= 0)
        prev = g_ptr_array_index(self->app_widgets,
                                 self->previously_activated_index);

    AppSwitcherAppWidget *last = NULL;
    if (self->last_activated_index >= 0)
        last = g_ptr_array_index(self->app_widgets, self->last_activated_index);

    GtkWidget *child =
        gtk_widget_get_first_child(GTK_WIDGET(self->app_widget_list));

    while (child) {
        AppSwitcherAppWidget *app_widget =
            g_object_get_data(G_OBJECT(child), "app-widget");

        g_ptr_array_add(self->app_widgets, app_widget);

        if (prev && app_widget == prev) {
            self->previously_activated_index = self->app_widgets->len - 1;
        }
        if (last && app_widget == last)
            self->last_activated_index = self->app_widgets->len - 1;

        child = gtk_widget_get_next_sibling(child);
    }
}

static void app_switcher_activate_widget_at_index(AppSwitcher *self,
                                                  gint index) {
    if (index < 0 || index >= self->app_widgets->len) {
        return;
    }

    AppSwitcherAppWidget *widget = g_ptr_array_index(self->app_widgets, index);
    app_switcher_app_widget_activate(widget);
}

static void on_top_level_changed(WaylandService *wayland, GPtrArray *toplevels,
                                 WaylandWLRForeignTopLevel *toplevel,
                                 AppSwitcher *self) {
    if (toplevel->app_id == NULL) return;

    AppSwitcherAppWidget *app_widget = NULL;
    gint index =
        app_switcher_find_widget_by_app_id(self, toplevel->app_id, &app_widget);

    if (app_widget) {
        // call into the AppWidget with the toplevel, it will know how to
        // update the existing widget.
        app_switcher_app_widget_add_toplevel(app_widget, toplevel);
    } else {
        g_debug(
            "app_switcher.c:on_top_level_changed: creating new widget: %s "
            "title: %s",
            toplevel->app_id, toplevel->title);
        app_widget = g_object_new(APP_SWITCHER_APP_WIDGET_TYPE, NULL);
        app_switcher_app_widget_add_toplevel(app_widget, toplevel);

        g_ptr_array_add(self->app_widgets, app_widget);
        index = self->app_widgets->len - 1;

        gtk_box_append(self->app_widget_list,
                       app_switcher_app_widget_get_widget(app_widget));
    }

    // if a toplevel was activated we want to move it to the front of the
    // app-switcher and move the previous accessed app to the second position.
    if (toplevel->activated) {
        if (self->last_activated_index >= 0) {
            self->previously_activated_index = self->last_activated_index;
            AppSwitcherAppWidget *prev = g_ptr_array_index(
                self->app_widgets, self->previously_activated_index);
            gtk_box_reorder_child_after(
                self->app_widget_list, app_switcher_app_widget_get_widget(prev),
                NULL);
        }

        self->last_activated_index = index;
        AppSwitcherAppWidget *last =
            g_ptr_array_index(self->app_widgets, self->last_activated_index);
        gtk_box_reorder_child_after(self->app_widget_list,
                                    app_switcher_app_widget_get_widget(last),
                                    NULL);

        // our data model and our displayed widget list maybe out of sync since
        // we moved the displayed widgets around, sync it.
        app_switcher_sync_display_widgets(self);
    }
}

static void app_switcher_focus_widget_at_index(AppSwitcher *self, gint index);

static void on_top_level_removed(WaylandService *wayland,
                                 WaylandWLRForeignTopLevel *toplevel,
                                 AppSwitcher *self) {
    if (toplevel->app_id == NULL) {
        g_critical("app_switcher.c:on_top_level_removed: app_id is NULL");
        return;
    }

    AppSwitcherAppWidget *widget = NULL;
    gint index =
        app_switcher_find_widget_by_app_id(self, toplevel->app_id, &widget);

    if (!widget) return;

    gboolean purge = app_switcher_app_widget_remove_toplevel(widget, toplevel);

    if (!purge) return;

    gtk_box_remove(self->app_widget_list,
                   app_switcher_app_widget_get_widget(widget));

    // get last activated index
    AppSwitcherAppWidget *last =
        g_ptr_array_index(self->app_widgets, self->last_activated_index);
    AppSwitcherAppWidget *prev =
        g_ptr_array_index(self->app_widgets, self->previously_activated_index);

    g_ptr_array_remove_index(self->app_widgets, index);
    g_object_unref(widget);

    // if len of app_widgets is zero just hide
    if (self->app_widgets->len == 0) {
        app_switcher_hide(self);
        self->last_activated_index = -1;
        self->previously_activated_index = -1;
        return;
    }

    // if the widget we are pruging is our last activate, and a previously
    // activated exist, set this as our last
    if (widget == last && prev) {
        self->last_activated_index = self->previously_activated_index;
        self->previously_activated_index = -1;
    }

    // if the widget we are purging is our last acitvated, and a previously
    // activated widget does not exist, set last activated index to zero
    if (widget == last) {
        self->last_activated_index = 0;
        self->previously_activated_index = -1;
    }

    if (widget == prev) {
        self->previously_activated_index = -1;
    }
}

static void app_switcher_init_layout(AppSwitcher *self) {
    self->win = ADW_WINDOW(adw_window_new());
    gtk_widget_set_hexpand(GTK_WIDGET(self->win), true);
    gtk_widget_set_vexpand(GTK_WIDGET(self->win), true);
    gtk_widget_set_name(GTK_WIDGET(self->win), "app-switcher");
    gtk_widget_set_visible(GTK_WIDGET(self->win), false);
    gtk_widget_add_controller(GTK_WIDGET(self->win), self->key_controller);

    // width zero to expand with content.
    gtk_window_set_default_size(GTK_WINDOW(self->win), 0, 0);

    g_signal_connect(self->win, "destroy", G_CALLBACK(on_window_destroy), self);

    // configure layershell, no anchors will place window in center.
    gtk_layer_init_for_window(GTK_WINDOW(self->win));
    gtk_layer_set_layer((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_anchor((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_EDGE_TOP,
                         true);
    gtk_layer_set_margin((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_EDGE_TOP,
                         500);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(self->win),
                                GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);

    self->app_widget_list = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->app_widget_list), "app-switcher-list");

    adw_window_set_content(self->win, GTK_WIDGET(self->app_widget_list));

    // listen for toplevels from wayland service
    WaylandService *wayland = wayland_service_get_global();

    g_signal_connect(wayland, "top-level-changed",
                     G_CALLBACK(on_top_level_changed), self);

    g_signal_connect(wayland, "top-level-removed",
                     G_CALLBACK(on_top_level_removed), self);
}

void app_switcher_unfocus_widget_all(AppSwitcher *self) {
    for (guint i = 0; i < self->app_widgets->len; i++) {
        AppSwitcherAppWidget *widget = g_ptr_array_index(self->app_widgets, i);
        app_switcher_app_widget_unset_focus(widget);
    }
}

static void app_switcher_unfocus_widget_all_with_focus_reset(AppSwitcher *self) {
    app_switcher_unfocus_widget_all(self);
    self->focused_index = -1;
}

static void app_switcher_focus_widget_at_index(AppSwitcher *self, gint index) {
    if (index < 0 || index >= self->app_widgets->len) {
        return;
    }
    app_switcher_unfocus_widget_all_with_focus_reset(self);

    self->focused_index = index;

    AppSwitcherAppWidget *widget = g_ptr_array_index(self->app_widgets, index);
    app_switcher_app_widget_set_focused(widget);
}

static void select_next(AppSwitcher *self) {
    g_debug("app_switcher.c:select_next called");

    // if only one widget present just return
    if (self->app_widgets->len == 1) {
        return;
    }

    gint index = self->focused_index;
    index++;
    if (index >= self->app_widgets->len) {
        index = 0;
    }

    app_switcher_focus_widget_at_index(self, index);
}

static void select_previous(AppSwitcher *self) {
    // if only one widget present just return
    if (self->app_widgets->len == 1) {
        return;
    }

    gint index = self->focused_index;
    g_debug("app_switcher.c:select_previous: index: %d", index);
    index--;
    if (index < 0) {
        index = self->app_widgets->len - 1;
    }

    g_debug("app_switcher.c:select_previous: index: %d", index);
    app_switcher_focus_widget_at_index(self, index);
}

static void select_next_instance(AppSwitcher *self) {
    g_debug("app_switcher.c:select_next_instance called");

    if (!gtk_widget_is_visible(GTK_WIDGET(self->win))) return;

    if (self->focused_index < 0) return;

    AppSwitcherAppWidget *widget =
        g_ptr_array_index(self->app_widgets, self->focused_index);

    app_switcher_app_widget_set_focused_next_instance(widget);
}

static void select_prev_instance(AppSwitcher *self) {
    g_debug("app_switcher.c:select_prev_instance called");

    if (!gtk_widget_is_visible(GTK_WIDGET(self->win))) return;

    if (self->focused_index < 0) return;

    AppSwitcherAppWidget *widget =
        g_ptr_array_index(self->app_widgets, self->focused_index);

    app_switcher_app_widget_set_focused_prev_instance(widget);
}

gboolean key_pressed(GtkEventControllerKey *controller, guint keyval,
                     guint keycode, GdkModifierType state, AppSwitcher *self) {
    g_debug("app_switcher.c:key_pressed called");

    gint shift_super_mask = GDK_SHIFT_MASK | GDK_SUPER_MASK;

    gboolean is_super_shift = (state & shift_super_mask) == shift_super_mask;
    g_debug("is super shift: %d keyval: %d", is_super_shift, keyval);

    if (keyval == GDK_KEY_ISO_Left_Tab && is_super_shift) {
        select_previous(self);
        return true;
    }

    if (keyval == GDK_KEY_Tab && (state & GDK_SUPER_MASK)) {
        select_next(self);
        return true;
    }

    if (keyval == GDK_KEY_G && is_super_shift) {
        select_prev_instance(self);
        return true;
    }

    if (keyval == GDK_KEY_g && (state & GDK_SUPER_MASK)) {
        select_next_instance(self);
        return true;
    }

    if (keyval == GDK_KEY_Escape && (state & GDK_SUPER_MASK)) {
        app_switcher_hide(self);
        return true;
    }
    return false;
}

gboolean key_released(GtkEventControllerKey *controller, guint keyval,
                      guint keycode, GdkModifierType state, AppSwitcher *self) {
    g_debug("app_switcher.c:key_released called");
    if (keyval == GDK_KEY_Super_L || keyval == GDK_KEY_Super_R) {
        app_switcher_activate_widget_at_index(self, self->focused_index);
        app_switcher_hide(self);
        return true;
    }
    return false;
}

static void app_switcher_init(AppSwitcher *self) {
    self->app_widgets = g_ptr_array_new();
    self->focused_index = -1;
    self->last_activated_index = -1;
    self->previously_activated_index = -1;

    self->key_controller = gtk_event_controller_key_new();

    g_signal_connect(self->key_controller, "key-pressed",
                     G_CALLBACK(key_pressed), self);
    g_signal_connect(self->key_controller, "key-released",
                     G_CALLBACK(key_released), self);

    app_switcher_init_layout(self);
}

AppSwitcher *app_switcher_get_global() {
    if (!global) {
        global = g_object_new(APP_SWITCHER_TYPE, NULL);
    }
    return global;
}

void app_switcher_activate(AdwApplication *app, gpointer user_data) {
    global = g_object_new(APP_SWITCHER_TYPE, NULL);
}

void app_switcher_show(AppSwitcher *self) {
    g_debug("app_switcher.c:app_switcher_show called");

    if (self->app_widgets->len == 0) {
        return;
    }

    if (self->previously_activated_index >= 0) {
        // focus previously activated
        app_switcher_focus_widget_at_index(self,
                                           self->previously_activated_index);
    } else {
        // focus last activated
        app_switcher_focus_widget_at_index(self, self->last_activated_index);
    }

    gtk_window_present(GTK_WINDOW(self->win));
    WaylandService *wayland = wayland_service_get_global();
    wayland_wlr_shortcuts_inhibitor_create(wayland, GTK_WIDGET(self->win));

    app_switcher_enter_preview(self);
}

void app_switcher_hide(AppSwitcher *self) {
    g_debug("app_switcher.c:app_switcher_hide called");

    app_switcher_unfocus_widget_all_with_focus_reset(self);

    gtk_widget_set_visible(GTK_WIDGET(self->win), false);
    WaylandService *wayland = wayland_service_get_global();
    wayland_wlr_shortcuts_inhibitor_destroy(wayland);

    app_switcher_exit_preview(self);
}

void app_switcher_toggle(AppSwitcher *self) {
    if (gtk_widget_get_visible(GTK_WIDGET(self->win))) {
        app_switcher_hide(self);
    } else {
        app_switcher_show(self);
    }
}

void app_switcher_focus_by_app_widget(AppSwitcher *self,
                                      AppSwitcherAppWidget *widget) {
    guint index;
    gboolean found = g_ptr_array_find(self->app_widgets, widget, &index);

    if (!found) return;

    app_switcher_focus_widget_at_index(self, index);
}

GtkWindow *app_switcher_get_window(AppSwitcher *self) {
    return GTK_WINDOW(self->win);
}

void app_switcher_enter_preview(AppSwitcher *self) {
    WaylandService *wayland = wayland_service_get_global();
    g_signal_handlers_block_by_func(wayland, on_top_level_changed, self);
}

void app_switcher_exit_preview(AppSwitcher *self) {
    WaylandService *wayland = wayland_service_get_global();
    g_signal_handlers_unblock_by_func(wayland, on_top_level_changed, self);
}
