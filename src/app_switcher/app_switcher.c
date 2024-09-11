#include "./app_switcher.h"

#include <adwaita.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

#include "./../services/wayland/foreign_toplevel_service/foreign_toplevel.h"
#include "./../services/wayland/keyboard_shortcuts_inhibit_service/ksi.h"
#include "./app_switcher_app_widget.h"
#include "gdk/gdkkeysyms.h"
#include "gtk/gtk.h"

static AppSwitcher *global = NULL;

enum signals { signals_n };

typedef struct _AppSwitcher {
    GObject parent_instance;
    AdwWindow *win;
    GtkScrolledWindow *scrolled;
    GtkBox *app_widget_list;
    GtkEventController *key_controller;
    int widget_n;
    gint focused_index;
    gchar *last_activated_app;
    gpointer last_activated_instance;
    gboolean select_alternative_app;
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

    WaylandKSIService *ksi = wayland_ksi_service_get_global();
    wayland_ksi_inhibit_destroy(ksi);

    gtk_widget_remove_controller(GTK_WIDGET(self->win), self->key_controller);

    app_switcher_init_layout(self);
}

AppSwitcherAppWidget *app_switcher_get_widget_at_index(AppSwitcher *self,
                                                       int i) {
    int ii = 0;

    AppSwitcherAppWidget *app_widget = NULL;

    GtkWidget *w =
        gtk_widget_get_first_child(GTK_WIDGET(self->app_widget_list));
    while (w) {
        if (ii == i) {
            app_widget = g_object_get_data(G_OBJECT(w), "app-widget");
            break;
        }
        w = gtk_widget_get_next_sibling(w);
        ii++;
    }

    return app_widget;
}

gint app_switcher_find_widget_by_app_id(AppSwitcher *self, gchar *app_id,
                                        AppSwitcherAppWidget **widget) {
    int ii = 0;

    AppSwitcherAppWidget *app_widget = NULL;

    GtkWidget *w =
        gtk_widget_get_first_child(GTK_WIDGET(self->app_widget_list));
    while (w) {
        app_widget = g_object_get_data(G_OBJECT(w), "app-widget");

        if (strcmp(app_switcher_app_widget_get_app_id(app_widget), app_id) ==
            0) {
            *widget = app_widget;
            return ii;
        }

        w = gtk_widget_get_next_sibling(w);
        ii++;
    }

    return -1;
}

static void app_switcher_activate_widget_at_index(AppSwitcher *self,
                                                  gint index) {
    AppSwitcherAppWidget *widget =
        app_switcher_get_widget_at_index(self, index);
    if (!widget) {
        return;
    }
    app_switcher_app_widget_activate(widget);
}

static void on_top_level_changed(WaylandForeignToplevelService *wayland,
                                 GHashTable *toplevels,
                                 WaylandWLRForeignTopLevel *toplevel,
                                 AppSwitcher *self) {
    AppSwitcherAppWidget *app_widget = NULL;
    app_switcher_find_widget_by_app_id(self, toplevel->app_id, &app_widget);

    if (!app_widget) {
        g_debug(
            "app_switcher.c:on_top_level_changed: creating new widget: %s "
            "title: %s",
            toplevel->app_id, toplevel->title);

        app_widget = g_object_new(APP_SWITCHER_APP_WIDGET_TYPE, NULL);

        gtk_box_append(self->app_widget_list,
                       app_switcher_app_widget_get_widget(app_widget));

        self->widget_n++;
    }

    app_switcher_app_widget_add_toplevel(app_widget, toplevel);

    if (toplevel->activated) {
        gtk_box_reorder_child_after(
            self->app_widget_list,
            app_switcher_app_widget_get_widget(app_widget), NULL);

        // a bit subtle but this checks to see if the latest activated app
        // matches the previously activated app, but a different instance of it.
        //
        // in this case, the next time the app-switcher opens, we don't want to
        // automatically focus the previous activiated app.
        // this ensures that if you activate instances of the same app the
        // app-switcher will open to select the previous instance of the app,
        // instead of the previously activated (separate) app.
        if (self->last_activated_app) {
            if (strcmp(self->last_activated_app, toplevel->app_id) == 0 &&
                self->last_activated_instance != toplevel->toplevel)
                self->select_alternative_app = false;
            else
                self->select_alternative_app = true;
            g_free(self->last_activated_app);
        }

        self->last_activated_app = g_strdup(toplevel->app_id);
        self->last_activated_instance = toplevel->toplevel;
    }
}

static void app_switcher_focus_widget_at_index(AppSwitcher *self, gint index);

static void on_top_level_removed(WaylandForeignToplevelService *wayland,
                                 WaylandWLRForeignTopLevel *toplevel,
                                 AppSwitcher *self) {
    if (toplevel->app_id == NULL) {
        g_critical("app_switcher.c:on_top_level_removed: app_id is NULL");
        return;
    }

    AppSwitcherAppWidget *widget = NULL;
    app_switcher_find_widget_by_app_id(self, toplevel->app_id, &widget);

    if (!widget) return;

    gboolean purge = app_switcher_app_widget_remove_toplevel(widget, toplevel);

    if (!purge) return;

    gtk_box_remove(self->app_widget_list,
                   app_switcher_app_widget_get_widget(widget));

    self->widget_n--;
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
    gtk_layer_set_namespace(GTK_WINDOW(self->win), "way-shell-switcher");
    gtk_layer_set_layer((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_EDGE_TOP,
                         true);
    gtk_layer_set_margin((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_EDGE_TOP,
                         500);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(self->win),
                                GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);

    self->scrolled = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
    gtk_scrolled_window_set_max_content_width(self->scrolled, 1400);
    gtk_scrolled_window_set_max_content_height(self->scrolled, -1);
    gtk_scrolled_window_set_propagate_natural_height(self->scrolled, 1);
    gtk_scrolled_window_set_propagate_natural_width(self->scrolled, 1);

    self->app_widget_list = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->app_widget_list), "app-switcher-list");

    gtk_scrolled_window_set_child(self->scrolled,
                                  GTK_WIDGET(self->app_widget_list));

    adw_window_set_content(self->win, GTK_WIDGET(self->scrolled));

    // listen for toplevels from wayland service
    WaylandForeignToplevelService *wayland =
        wayland_foreign_toplevel_service_get_global();

    GHashTable *toplevels =
        wayland_foreign_toplevel_service_get_toplevels(wayland);
    GList *values = g_hash_table_get_values(toplevels);
    for (GList *l = values; l != NULL; l = l->next) {
        WaylandWLRForeignTopLevel *toplevel = l->data;
        on_top_level_changed(wayland, toplevels, toplevel, self);
    }
    g_list_free(values);

    g_signal_connect(wayland, "top-level-changed",
                     G_CALLBACK(on_top_level_changed), self);

    g_signal_connect(wayland, "top-level-removed",
                     G_CALLBACK(on_top_level_removed), self);
}

void app_switcher_unfocus_widget_all(AppSwitcher *self) {
    GtkWidget *w =
        gtk_widget_get_first_child(GTK_WIDGET(self->app_widget_list));
    while (w) {
        AppSwitcherAppWidget *widget =
            g_object_get_data(G_OBJECT(w), "app-widget");
        app_switcher_app_widget_unset_focus(widget);
        w = gtk_widget_get_next_sibling(w);
    }
}

static void app_switcher_unfocus_widget_all_with_focus_reset(
    AppSwitcher *self) {
    app_switcher_unfocus_widget_all(self);
    self->focused_index = -1;
}

static void app_switcher_focus_widget_at_index(AppSwitcher *self, gint index) {
    app_switcher_unfocus_widget_all_with_focus_reset(self);

    self->focused_index = index;

    AppSwitcherAppWidget *widget =
        app_switcher_get_widget_at_index(self, index);

    // don't automatically focus previous instance of selected app, this
    // avoids the case where switching between two apps also swaps the most
    // recently activated instances of an app.
    gboolean select_previous = true;
    if (self->last_activated_app) {
        if (strcmp(self->last_activated_app,
                   app_switcher_app_widget_get_app_id(widget)) != 0)
            select_previous = false;
    }

    app_switcher_app_widget_set_focused(widget, select_previous);
}

static void select_next(AppSwitcher *self) {
    g_debug("app_switcher.c:select_next called");

    gint index = self->focused_index;
    index++;
    if (index >= self->widget_n) {
        index = 0;
    }

    app_switcher_focus_widget_at_index(self, index);
}

static void select_previous(AppSwitcher *self) {
    gint index = self->focused_index;
    g_debug("app_switcher.c:select_previous: index: %d", index);
    index--;
    if (index < 0) {
        index = self->widget_n - 1;
    }

    app_switcher_focus_widget_at_index(self, index);
}

static void select_next_instance(AppSwitcher *self) {
    g_debug("app_switcher.c:select_next_instance called");

    if (!gtk_widget_is_visible(GTK_WIDGET(self->win))) return;

    if (self->focused_index < 0) return;

    AppSwitcherAppWidget *widget =
        app_switcher_get_widget_at_index(self, self->focused_index);

    app_switcher_app_widget_set_focused_next_instance(widget);
}

static void select_prev_instance(AppSwitcher *self) {
    g_debug("app_switcher.c:select_prev_instance called");

    if (!gtk_widget_is_visible(GTK_WIDGET(self->win))) return;

    if (self->focused_index < 0) return;

    AppSwitcherAppWidget *widget =
        app_switcher_get_widget_at_index(self, self->focused_index);

    app_switcher_app_widget_set_focused_prev_instance(widget);
}

static gboolean key_pressed(GtkEventControllerKey *controller, guint keyval,
                            guint keycode, GdkModifierType state,
                            AppSwitcher *self) {
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

static gboolean key_released(GtkEventControllerKey *controller, guint keyval,
                             guint keycode, GdkModifierType state,
                             AppSwitcher *self) {
    g_debug("app_switcher.c:key_released called");
    if (keyval == GDK_KEY_Super_L || keyval == GDK_KEY_Super_R) {
        app_switcher_activate_widget_at_index(self, self->focused_index);
        app_switcher_hide(self);
        return true;
    }
    return false;
}

static void app_switcher_init(AppSwitcher *self) {
    self->focused_index = -1;

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

    if (self->widget_n > 1) {
        if (self->select_alternative_app)
            app_switcher_focus_widget_at_index(self, 1);
        else
            app_switcher_focus_widget_at_index(self, 0);
    } else {
        app_switcher_focus_widget_at_index(self, 0);
    }

    gtk_window_present(GTK_WINDOW(self->win));
    WaylandKSIService *ksi = wayland_ksi_service_get_global();
    wayland_ksi_inhibit(ksi, GTK_WIDGET(self->win));

    app_switcher_enter_preview(self);
}

void app_switcher_hide(AppSwitcher *self) {
    g_debug("app_switcher.c:app_switcher_hide called");

    app_switcher_unfocus_widget_all_with_focus_reset(self);

    gtk_widget_set_visible(GTK_WIDGET(self->win), false);
    WaylandKSIService *ksi = wayland_ksi_service_get_global();
    wayland_ksi_inhibit_destroy(ksi);

    app_switcher_exit_preview(self);
}

void app_switcher_toggle(AppSwitcher *self) {
    if (gtk_widget_get_visible(GTK_WIDGET(self->win))) {
        app_switcher_hide(self);
    } else {
        app_switcher_show(self);
    }
}

GtkWindow *app_switcher_get_window(AppSwitcher *self) {
    return GTK_WINDOW(self->win);
}

void app_switcher_enter_preview(AppSwitcher *self) {
    WaylandForeignToplevelService *foreign_toplevel =
        wayland_foreign_toplevel_service_get_global();
    g_signal_handlers_block_by_func(foreign_toplevel, on_top_level_changed,
                                    self);
}

void app_switcher_exit_preview(AppSwitcher *self) {
    WaylandForeignToplevelService *foreign_toplevel =
        wayland_foreign_toplevel_service_get_global();
    g_signal_handlers_unblock_by_func(foreign_toplevel, on_top_level_changed,
                                      self);
}
