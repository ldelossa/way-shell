
#include "./app_switcher_app_widget.h"

#include <adwaita.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

#include "./../services/wayland_service/wayland_service.h"
#include "./app_switcher.h"
#include "glibconfig.h"

enum signals { signals_n };

typedef struct mouse_coords {
    double x;
    double y;
} mouse_coords;

typedef struct _AppSwitcherAppWidget {
    GObject parent_instance;
    AppSwitcherAppWidget *parent;
    gpointer wl_toplevel;
    gchar *app_id;
    AdwWindow *win;
    GtkEventControllerMotion *ctrl;
    GtkBox *container;
    GtkButton *button;
    GtkBox *button_contents;
    GtkImage *icon;
    GtkLabel *id_or_title;
    GtkImage *expand_arrow;
    GtkBox *instances_container;
    GPtrArray *instances;
    mouse_coords mouse;
    gint focused_index;
    gint last_activated_index;
    gint previously_activated_index;
} AppSwitcherAppWidget;
static guint app_switcher_app_widget_signals[signals_n] = {0};
G_DEFINE_TYPE(AppSwitcherAppWidget, app_switcher_app_widget, G_TYPE_OBJECT);

gint app_switcher_app_widget_search_by_toplevel(
    AppSwitcherAppWidget *self, WaylandWLRForeignTopLevel *toplevel,
    AppSwitcherAppWidget **found) {
    for (gint i = 0; i < self->instances->len; i++) {
        AppSwitcherAppWidget *instance = g_ptr_array_index(self->instances, i);
        if (instance->wl_toplevel == toplevel->toplevel) {
            *found = instance;
            return i;
        }
    }
    return -1;
}

// stub out dispose, finalize, class_init and init methods.
static void app_switcher_app_widget_dispose(GObject *object) {
    G_OBJECT_CLASS(app_switcher_app_widget_parent_class)->dispose(object);
}

static void app_switcher_app_widget_finalize(GObject *object) {
    G_OBJECT_CLASS(app_switcher_app_widget_parent_class)->finalize(object);
}

static void app_switcher_app_widget_class_init(
    AppSwitcherAppWidgetClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = app_switcher_app_widget_dispose;
    object_class->finalize = app_switcher_app_widget_finalize;
}

static void on_button_clicked(GtkButton *button, AppSwitcherAppWidget *self) {
    g_debug("app_switcher_app_widget:on_button_clicked called");

    AppSwitcher *app_switcher = app_switcher_get_global();
    app_switcher_hide(app_switcher);

    WaylandService *wayland = wayland_service_get_global();
    WaylandWLRForeignTopLevel tp = {
        .toplevel = self->wl_toplevel,
    };
    wayland_wlr_foreign_toplevel_activate(wayland, &tp);
}

static void app_switcher_app_widget_init_layout(AppSwitcherAppWidget *self) {
    self->win = ADW_WINDOW(adw_window_new());
    gtk_widget_set_name(GTK_WIDGET(self->win), "app-switcher");
    gtk_window_set_default_size(GTK_WINDOW(self->win), 0, 0);
    gtk_widget_set_visible(GTK_WIDGET(self->win), false);

    // configure layershell, no anchors will place window in center.
    gtk_layer_init_for_window(GTK_WINDOW(self->win));
    gtk_layer_set_layer((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_anchor((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_EDGE_TOP,
                         true);
    gtk_layer_set_margin((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_EDGE_TOP,
                         660);

    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    // stash this here for easier access in AppSwitcher
    g_object_set_data(G_OBJECT(self->container), "app-widget", self);
    gtk_widget_add_css_class(GTK_WIDGET(self->container),
                             "app-switcher-app-widget");

    self->button = GTK_BUTTON(gtk_button_new());
    g_signal_connect(self->button, "clicked", G_CALLBACK(on_button_clicked),
                     self);
    gtk_widget_add_css_class(GTK_WIDGET(self->button),
                             "app-switcher-app-widget-button");

    self->button_contents = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_halign(GTK_WIDGET(self->button_contents), GTK_ALIGN_CENTER);
    gtk_widget_set_valign(GTK_WIDGET(self->button_contents), GTK_ALIGN_CENTER);

    self->icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("application-x-executable"));
    gtk_image_set_pixel_size(self->icon, 64);

    self->expand_arrow =
        GTK_IMAGE(gtk_image_new_from_icon_name("pan-down-symbolic"));
    gtk_widget_add_css_class(GTK_WIDGET(self->expand_arrow),
                             "app-switcher-app-widget-expand-arrow");
    gtk_image_set_pixel_size(self->expand_arrow, 12);
    gtk_widget_set_visible(GTK_WIDGET(self->expand_arrow), false);

    self->id_or_title = GTK_LABEL(gtk_label_new(""));
    gtk_widget_add_css_class(GTK_WIDGET(self->id_or_title),
                             "app-switcher-app-widget-label");
    gtk_label_set_width_chars(self->id_or_title, 15);
    gtk_label_set_max_width_chars(self->id_or_title, 15);
    gtk_label_set_ellipsize(self->id_or_title, PANGO_ELLIPSIZE_START);
    gtk_label_set_xalign(self->id_or_title, 0.5);

    self->instances_container =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

    // wire the widget up
    gtk_box_append(self->button_contents, GTK_WIDGET(self->icon));
    gtk_box_append(self->button_contents, GTK_WIDGET(self->id_or_title));

    gtk_button_set_child(self->button, GTK_WIDGET(self->button_contents));
    gtk_box_append(self->container, GTK_WIDGET(self->button));
    gtk_box_append(self->container, GTK_WIDGET(self->expand_arrow));

    adw_window_set_content(self->win, GTK_WIDGET(self->instances_container));
}

static void app_switcher_app_widget_init(AppSwitcherAppWidget *self) {
    self->instances = g_ptr_array_new();
    self->ctrl = GTK_EVENT_CONTROLLER_MOTION(gtk_event_controller_motion_new());
    self->last_activated_index = -1;
    self->previously_activated_index = -1;
    self->focused_index = -1;
    app_switcher_app_widget_init_layout(self);
}

GAppInfo *search_apps_by_app_id(gchar *app_id) {
    GAppInfo *app_info = NULL;
    GList *apps = g_app_info_get_all();
    GList *l;
    g_debug("app_switcher_app_widget: search_apps_by_app_id: %s", app_id);
    for (l = apps; l != NULL; l = l->next) {
        GAppInfo *info = l->data;
        const gchar *id = g_app_info_get_id(info);
        const gchar *lower_id = g_utf8_strdown(id, -1);
        const gchar *lower_app_id = g_utf8_strdown(app_id, -1);
        if (g_strrstr(lower_id, lower_app_id)) {
            app_info = info;
            break;
        }
    }
    g_list_free(apps);
    return app_info;
}

static void set_icon(AppSwitcherAppWidget *self,
                     WaylandWLRForeignTopLevel *toplevel) {
    GAppInfo *app_info = search_apps_by_app_id(toplevel->app_id);
    GIcon *icon = g_app_info_get_icon(G_APP_INFO(app_info));
    // check and handle GFileIcon, set self->icon to a GtkImage
    if (G_IS_FILE_ICON(icon)) {
        g_debug("G_FILE_ICON");
    }
    // check and handle if GThemedIcon
    else if (G_IS_THEMED_ICON(icon)) {
        GdkDisplay *display = gdk_display_get_default();
        GtkIconTheme *theme = gtk_icon_theme_get_for_display(display);
        GtkIconPaintable *paintable = gtk_icon_theme_lookup_by_gicon(
            theme, icon, 64, 1, GTK_TEXT_DIR_RTL, 0);
        gtk_image_set_from_paintable(self->icon, GDK_PAINTABLE(paintable));
    }
    // check and handle if GLoadableIcon
    else if (G_IS_LOADABLE_ICON(icon)) {
        g_debug("G_LOADABLE_ICON");
    }
}

static void app_switcher_app_widget_set_layout_instance(
    AppSwitcherAppWidget *self) {
    gtk_image_set_pixel_size(self->icon, 48);
    gtk_label_set_width_chars(self->id_or_title, 14);
    gtk_label_set_max_width_chars(self->id_or_title, 14);
    gtk_widget_add_css_class(GTK_WIDGET(self->button), "instance");
}

gboolean app_switcher_app_widget_remove_toplevel(
    AppSwitcherAppWidget *self, WaylandWLRForeignTopLevel *toplevel) {
    AppSwitcherAppWidget *instance = NULL;
    gint index =
        app_switcher_app_widget_search_by_toplevel(self, toplevel, &instance);

    if (!instance) return false;

    // remove from instances container
    gtk_box_remove(self->instances_container,
                   app_switcher_app_widget_get_widget(instance));

    AppSwitcherAppWidget *last = NULL;
    if (self->last_activated_index >= 0)
        last = g_ptr_array_index(self->instances, self->last_activated_index);
    AppSwitcherAppWidget *prev = NULL;
    if (self->previously_activated_index >= 0)
        prev = g_ptr_array_index(self->instances,
                                 self->previously_activated_index);

    g_ptr_array_remove_index(self->instances, index);

    // if we are left with no instances, tell caller to purge the root widget.
    if (self->instances->len == 0) {
        return true;
    }

    // if we are left with only one instance, remove any index to previous
    // and set last activated to 0.
    if (self->instances->len == 1) {
        self->previously_activated_index = -1;
        self->last_activated_index = 0;
        gtk_widget_set_visible(GTK_WIDGET(self->expand_arrow), false);
        gtk_widget_set_visible(GTK_WIDGET(self->win), false);
        return false;
    }

    // if the widget we are removing is our most recently activated and we
    // have a previous one, make this one the activate.
    if (instance == last && prev) {
        self->last_activated_index = self->previously_activated_index;
        self->previously_activated_index = -1;
        return false;
    }

    // if its the most recently activated widget and we don't have a previous
    // set last activated index to 0
    if (instance == last) {
        self->last_activated_index = 0;
        self->previously_activated_index = -1;
        return false;
    }

    if (instance == prev) {
        self->previously_activated_index = -1;
        return false;
    }

    // if our root widget was the deleted one we need to replace the toplevel
    // of our root, follow the same preferences as above.
    if (self->wl_toplevel == toplevel->toplevel) {
        if (prev)
            self->wl_toplevel = prev->wl_toplevel;
        else if (last)
            self->wl_toplevel = last->wl_toplevel;
        else
            self->wl_toplevel =
                ((AppSwitcherAppWidget *)g_ptr_array_index(self->instances, 0))
                    ->wl_toplevel;
    }

    return false;
}

void app_switcher_app_widget_preview(AppSwitcherAppWidget *self) {
    AppSwitcher *app_switcher = app_switcher_get_global();
    GtkWindow *app_switcher_win = app_switcher_get_window(app_switcher);

    // activate toplevel to preview
    WaylandService *wayland = wayland_service_get_global();
    WaylandWLRForeignTopLevel tp = {
        .toplevel = self->wl_toplevel,
    };

    wayland_wlr_foreign_toplevel_activate(wayland, &tp);

    // this bit allows the Sway compositor to set the 'focused' border on the
    // window being previewed.
    //
    // we need to temporarily hide the app-switcher window so the Sway
    // compositor can set the 'focused border' color on the activated window
    // above.
    //
    // if we do not do this, the border of the activated window above remains
    // the 'unfocused border' color and the user cannot understand which window
    // is being previewed.
    gtk_widget_set_visible(GTK_WIDGET(app_switcher_win), false);
    gtk_widget_set_visible(GTK_WIDGET(app_switcher_win), true);
}

static void on_button_enter(GtkEventControllerMotion *ctlr, double x, double y,
                            AppSwitcherAppWidget *self) {
    g_debug("app_switcher_app_widget:on_button_enter called");

    // when performing of a preview we need to quickly hide the app-switcher
    // and return to it, this creates additional enter events we do not want.
    //
    // if the mouse has not moved since the last one, just return.
    //
    // we only need to do this in this function, because a Widget's instance
    // window does not need to open and close to get the preview effect, only
    // the main app-switcher window.
    if (self->mouse.x == x && self->mouse.y == y) return;

    AppSwitcher *app_switcher = app_switcher_get_global();
    app_switcher_unfocus_widget_all(app_switcher);
    app_switcher_app_widget_set_focused(self);

    app_switcher_app_widget_preview(self);

    self->mouse.x = x;
    self->mouse.y = y;
}

static void on_button_enter_instance(GtkEventControllerMotion *ctlr, double x,
                                     double y, AppSwitcherAppWidget *self) {
    g_debug("app_switcher_app_widget:on_button_enter called");

    app_switcher_app_widget_preview(self);
}

static void app_switcher_app_widget_sync_display_widgets(
    AppSwitcherAppWidget *self) {
    g_ptr_array_set_size(self->instances, 0);

    AppSwitcherAppWidget *prev = NULL;
    if (self->previously_activated_index >= 0)
        prev = g_ptr_array_index(self->instances,
                                 self->previously_activated_index);

    AppSwitcherAppWidget *last = NULL;
    if (self->last_activated_index >= 0)
        last = g_ptr_array_index(self->instances, self->last_activated_index);

    GtkWidget *child =
        gtk_widget_get_first_child(GTK_WIDGET(self->instances_container));

    while (child) {
        AppSwitcherAppWidget *app_widget =
            g_object_get_data(G_OBJECT(child), "app-widget");

        g_ptr_array_add(self->instances, app_widget);

        if (prev && app_widget == prev) {
            self->previously_activated_index = self->instances->len - 1;
        }
        if (last && app_widget == last)
            self->last_activated_index = self->instances->len - 1;

        child = gtk_widget_get_next_sibling(child);
    }
}

void app_switcher_app_widget_add_toplevel(AppSwitcherAppWidget *self,
                                          WaylandWLRForeignTopLevel *toplevel) {
    // if we have no previous top levels, configure ourselves
    if (self->instances->len == 0) {
        self->wl_toplevel = toplevel->toplevel;
        set_icon(self, toplevel);

        // we only connect the controller on high level instances
        gtk_widget_add_controller(GTK_WIDGET(self->button),
                                  GTK_EVENT_CONTROLLER(self->ctrl));
        g_signal_connect(self->ctrl, "enter", G_CALLBACK(on_button_enter),
                         self);

        self->app_id = strdup(toplevel->app_id);
        gtk_label_set_text(self->id_or_title, self->app_id);
        if (toplevel->activated) {
            self->last_activated_index = 0;
        }
    }

    // we have top levels, lets see if we have an instance of this toplevel,
    // if not create it.
    AppSwitcherAppWidget *instance = NULL;
    gint index =
        app_switcher_app_widget_search_by_toplevel(self, toplevel, &instance);
    if (!instance) {
        instance = g_object_new(APP_SWITCHER_APP_WIDGET_TYPE, NULL);
        instance->wl_toplevel = toplevel->toplevel;
        instance->parent = self;
        self->app_id = strdup(toplevel->app_id);

        gtk_widget_add_controller(GTK_WIDGET(instance->button),
                                  GTK_EVENT_CONTROLLER(instance->ctrl));
        g_signal_connect(instance->ctrl, "enter",
                         G_CALLBACK(on_button_enter_instance), instance);

        app_switcher_app_widget_set_layout_instance(instance);
        set_icon(instance, toplevel);
        gtk_box_append(self->instances_container,
                       app_switcher_app_widget_get_widget(instance));
        g_ptr_array_add(self->instances, instance);
        index = self->instances->len - 1;
    }

    // whether we created an instance or have an existing, this add maybe
    // just to update the widget's titles, do this in both cases.
    gtk_label_set_text(instance->id_or_title, toplevel->title);
    gtk_widget_set_tooltip_text(GTK_WIDGET(instance->button), toplevel->title);

    if (self->instances->len > 1) {
        gtk_widget_set_visible(GTK_WIDGET(self->expand_arrow), true);
    }

    if (toplevel->activated) {
        if (self->last_activated_index >= 0) {
            self->previously_activated_index = self->last_activated_index;
            AppSwitcherAppWidget *prev = g_ptr_array_index(
                self->instances, (guint)self->previously_activated_index);
            gtk_box_reorder_child_after(
                self->instances_container,
                app_switcher_app_widget_get_widget(prev), NULL);
        }

        // move most recently activated widget to front.
        self->last_activated_index = index;
        AppSwitcherAppWidget *last =
            g_ptr_array_index(self->instances, self->last_activated_index);
        gtk_box_reorder_child_after(self->instances_container,
                                    app_switcher_app_widget_get_widget(last),
                                    NULL);

        app_switcher_app_widget_sync_display_widgets(self);
    }
}

void app_switcher_app_widget_unset_focus(AppSwitcherAppWidget *self) {
    gtk_widget_set_visible(GTK_WIDGET(self->win), false);
    gtk_widget_remove_css_class(GTK_WIDGET(self->button), "selected");

    // unfocus all instances
    for (guint i = 0; i < self->instances->len; i++) {
        AppSwitcherAppWidget *instance = g_ptr_array_index(self->instances, i);
        gtk_widget_remove_css_class(GTK_WIDGET(instance->button), "selected");
    }
}

void app_switcher_app_widget_set_focused(AppSwitcherAppWidget *self) {
    app_switcher_app_widget_unset_focus(self);

    if (self->instances->len > 1) {
        // if this root widget has instances, highlight the previously selected
        // one.
        if (self->previously_activated_index >= 0) {
            AppSwitcherAppWidget *instance = g_ptr_array_index(
                self->instances, self->previously_activated_index);
            gtk_widget_add_css_class(GTK_WIDGET(instance->button), "selected");
            self->focused_index = self->previously_activated_index;
        }
        // present additional window which lists app instances.
        gtk_window_present(GTK_WINDOW(self->win));
    }

    gtk_widget_add_css_class(GTK_WIDGET(self->button), "selected");
}

void app_switcher_app_widget_set_focused_next_instance(
    AppSwitcherAppWidget *self) {
    if (self->instances->len <= 1) return;

    if (self->previously_activated_index < 0) return;

    AppSwitcherAppWidget *prev = g_ptr_array_index(
        self->instances, (guint)self->previously_activated_index);
    if (!prev) return;

    gint index = self->previously_activated_index;
    index++;
    if (index >= self->instances->len) {
        index = 0;
    }

    AppSwitcherAppWidget *instance = g_ptr_array_index(self->instances, index);
    if (!instance) return;

    app_switcher_app_widget_unset_focus(prev);
    app_switcher_app_widget_set_focused(instance);

    self->previously_activated_index = index;

    app_switcher_app_widget_preview(instance);
}

void app_switcher_app_widget_set_focused_prev_instance(
    AppSwitcherAppWidget *self) {
    if (self->instances->len <= 1) return;

    if (self->previously_activated_index < 0) return;

    AppSwitcherAppWidget *prev = g_ptr_array_index(
        self->instances, (guint)self->previously_activated_index);
    if (!prev) return;

    gint index = self->previously_activated_index;
    index--;
    if (index < 0) {
        index = self->instances->len - 1;
    }

    AppSwitcherAppWidget *instance = g_ptr_array_index(self->instances, index);
    if (!instance) return;

    app_switcher_app_widget_unset_focus(prev);
    app_switcher_app_widget_set_focused(instance);

    self->previously_activated_index = index;

    app_switcher_app_widget_preview(instance);
}

void app_switcher_app_widget_activate(AppSwitcherAppWidget *self) {
    WaylandService *wayland = wayland_service_get_global();

    WaylandWLRForeignTopLevel tp = {
        .toplevel = self->wl_toplevel,
    };

    if (self->previously_activated_index >= 0) {
        AppSwitcherAppWidget *instance = g_ptr_array_index(
            self->instances, self->previously_activated_index);
        if (instance) {
            tp.toplevel = instance->wl_toplevel;
            wayland_wlr_foreign_toplevel_activate(wayland, &tp);
            return;
        }
    }

    if (self->last_activated_index >= 0) {
        AppSwitcherAppWidget *instance =
            g_ptr_array_index(self->instances, self->last_activated_index);
        if (instance) {
            tp.toplevel = instance->wl_toplevel;
            wayland_wlr_foreign_toplevel_activate(wayland, &tp);
            return;
        }
    }

    // finaly, just activate ourselves if all else fails
    wayland_wlr_foreign_toplevel_activate(wayland, &tp);
}

void app_switcher_app_widget_update_last_activated(
    AppSwitcherAppWidget *self, WaylandWLRForeignTopLevel *toplevel) {
    AppSwitcherAppWidget *instance = NULL;
    gint index =
        app_switcher_app_widget_search_by_toplevel(self, toplevel, &instance);

    if (self->last_activated_index >= 0) {
        self->previously_activated_index = self->last_activated_index;
        AppSwitcherAppWidget *prev = g_ptr_array_index(
            self->instances, (guint)self->previously_activated_index);
        gtk_box_reorder_child_after(self->instances_container,
                                    app_switcher_app_widget_get_widget(prev),
                                    NULL);
    }

    // move most recently activated widget to front.
    self->last_activated_index = index;
    AppSwitcherAppWidget *last =
        g_ptr_array_index(self->instances, self->last_activated_index);
    gtk_box_reorder_child_after(self->instances_container,
                                app_switcher_app_widget_get_widget(last), NULL);

    // due to moving around our displayed widgets, our data model maybe out
    // of sync, sync it.
    app_switcher_app_widget_sync_display_widgets(self);
}

GtkWidget *app_switcher_app_widget_get_widget(AppSwitcherAppWidget *self) {
    return GTK_WIDGET(self->container);
}

gchar *app_switcher_app_widget_get_app_id(AppSwitcherAppWidget *self) {
    return self->app_id;
}
