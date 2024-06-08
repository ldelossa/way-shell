#include "./app_switcher_app_widget.h"

#include <adwaita.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

#include "./../services/wayland_service/wayland_service.h"
#include "./app_switcher.h"

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
    mouse_coords mouse;
    gint focused_index;
    int instances_n;
} AppSwitcherAppWidget;
static guint app_switcher_app_widget_signals[signals_n] = {0};
G_DEFINE_TYPE(AppSwitcherAppWidget, app_switcher_app_widget, G_TYPE_OBJECT);

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

AppSwitcherAppWidget *find_instance_by_toplevel(AppSwitcherAppWidget *self,
                                                gpointer toplevel) {
    GtkWidget *w =
        gtk_widget_get_first_child(GTK_WIDGET(self->instances_container));
    while (w) {
        AppSwitcherAppWidget *instance =
            g_object_get_data(G_OBJECT(w), "app-widget");

        if (instance->wl_toplevel == toplevel) {
            return instance;
        }
        w = gtk_widget_get_next_sibling(w);
    }
    return NULL;
}

AppSwitcherAppWidget *find_instance_by_index(AppSwitcherAppWidget *self,
                                             int i) {
    int ii = 0;
    GtkWidget *w =
        gtk_widget_get_first_child(GTK_WIDGET(self->instances_container));
    while (w) {
        if (ii == i) {
            AppSwitcherAppWidget *instance =
                g_object_get_data(G_OBJECT(w), "app-widget");
            return instance;
        }
        w = gtk_widget_get_next_sibling(w);
        ii++;
    }
    return NULL;
}

// this ties our class object's lifecycel to the owning container.
static void on_container_destroyed(GtkWidget *widget,
                                   AppSwitcherAppWidget *self) {
    g_object_unref(self);
}

static void app_switcher_app_widget_init_layout(AppSwitcherAppWidget *self) {
    self->win = ADW_WINDOW(adw_window_new());
    gtk_widget_set_name(GTK_WIDGET(self->win), "app-switcher");
    gtk_window_set_default_size(GTK_WINDOW(self->win), 0, 0);
    gtk_widget_set_visible(GTK_WIDGET(self->win), false);

    // configure layershell, no anchors will place window in center.
    gtk_layer_init_for_window(GTK_WINDOW(self->win));
    gtk_layer_set_namespace(GTK_WINDOW(self->win), "way-shell-switcher");
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
    g_signal_connect(self->container, "destroy",
                     G_CALLBACK(on_container_destroyed), self);

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
    self->ctrl = GTK_EVENT_CONTROLLER_MOTION(gtk_event_controller_motion_new());
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
    AppSwitcherAppWidget *instance =
        find_instance_by_toplevel(self, toplevel->toplevel);

    if (!instance) return false;

    // remove from instances container
    gtk_box_remove(self->instances_container,
                   app_switcher_app_widget_get_widget(instance));

    self->instances_n--;
    g_debug("app_switcher_app_widget_remove_toplevel: instances_n: %d",
            self->instances_n);

    if (self->instances_n == 0) {
        g_debug("app_switcher_app_widget_remove_toplevel: instances_n == 0");
        return true;
    }

    if (self->instances_n == 1) {
        gtk_widget_set_visible(GTK_WIDGET(self->expand_arrow), false);
        gtk_widget_set_visible(GTK_WIDGET(self->win), false);
        return false;
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
    if (self->parent && self->parent->win)
        gtk_widget_set_visible(GTK_WIDGET(self->parent->win), false);
    gtk_widget_set_visible(GTK_WIDGET(app_switcher_win), true);
    if (self->parent && self->parent->win)
        gtk_widget_set_visible(GTK_WIDGET(self->parent->win), true);
}

void app_switcher_app_widget_add_toplevel(AppSwitcherAppWidget *self,
                                          WaylandWLRForeignTopLevel *toplevel) {
    // first time we are setting a top level, configure our name and icon.
    if (!self->app_id) {
        set_icon(self, toplevel);
        self->app_id = strdup(toplevel->app_id);
        gtk_label_set_text(self->id_or_title, self->app_id);
        self->focused_index = 0;
    }

    // create instance specific for top-level
    AppSwitcherAppWidget *instance =
        find_instance_by_toplevel(self, toplevel->toplevel);
    if (!instance) {
        instance = g_object_new(APP_SWITCHER_APP_WIDGET_TYPE, NULL);
        instance->wl_toplevel = toplevel->toplevel;
        instance->parent = self;
        instance->app_id = strdup(toplevel->app_id);
        app_switcher_app_widget_init_layout(instance);
        app_switcher_app_widget_set_layout_instance(instance);
        set_icon(instance, toplevel);
        gtk_box_append(self->instances_container,
                       app_switcher_app_widget_get_widget(instance));
        self->instances_n++;
    }

    // whether we created an instance or have an existing, this add maybe
    // just to update the widget's titles, do this in both cases.
    gtk_label_set_text(instance->id_or_title, toplevel->title);
    gtk_widget_set_tooltip_text(GTK_WIDGET(instance->button), toplevel->title);

    if (self->instances_n > 1) {
        gtk_widget_set_visible(GTK_WIDGET(self->expand_arrow), true);
    }

    if (toplevel->activated) {
        gtk_box_reorder_child_after(
            self->instances_container,
            app_switcher_app_widget_get_widget(instance), 0);
    }
}

void app_switcher_app_widget_unset_focus(AppSwitcherAppWidget *self) {
    if (self->win) {
        gtk_widget_set_visible(GTK_WIDGET(self->win), false);
    }

    gtk_widget_remove_css_class(GTK_WIDGET(self->button), "selected");

    GtkWidget *w =
        gtk_widget_get_first_child(GTK_WIDGET(self->instances_container));
    while (w) {
        AppSwitcherAppWidget *instance =
            g_object_get_data(G_OBJECT(w), "app-widget");
        gtk_widget_remove_css_class(GTK_WIDGET(instance->button), "selected");
        w = gtk_widget_get_next_sibling(w);
    }
}

void app_switcher_app_widget_set_focused(AppSwitcherAppWidget *self,
                                         gboolean select_previous) {
    app_switcher_app_widget_unset_focus(self);

    if (self->instances_n > 1) {
        AppSwitcherAppWidget *instance = find_instance_by_index(self, 1);
        if (instance) {
            app_switcher_app_widget_set_focused(instance, true);
            gtk_window_present(GTK_WINDOW(self->win));
            if (select_previous)
                self->focused_index = 1;
            else
                self->focused_index = 0;
        }
    } else {
        self->focused_index = 0;
    }

    gtk_widget_add_css_class(GTK_WIDGET(self->button), "selected");
}

void app_switcher_app_widget_set_focused_next_instance(
    AppSwitcherAppWidget *self) {
    g_debug(
        "app_switcher_app_widget.c:app_switcher_app_widget_set_focused_next_"
        "instance() called");
    if (self->instances_n <= 1) return;

    gint index = self->focused_index;
    index++;
    if (index >= self->instances_n) {
        index = 0;
    }

    AppSwitcherAppWidget *instance = find_instance_by_index(self, index);

    if (!instance) return;

    app_switcher_app_widget_unset_focus(self);

    app_switcher_app_widget_set_focused(instance, true);

    self->focused_index = index;

    app_switcher_app_widget_preview(instance);
}

void app_switcher_app_widget_set_focused_prev_instance(
    AppSwitcherAppWidget *self) {
    g_debug(
        "app_switcher_app_widget.c:app_switcher_app_widget_set_focused_prev_"
        "instance() called");
    if (self->instances_n <= 1) return;

    gint index = self->focused_index;
    index--;
    if (index < 0) {
        index = self->instances_n - 1;
    }

    AppSwitcherAppWidget *instance = find_instance_by_index(self, index);
    if (!instance) return;

    app_switcher_app_widget_unset_focus(self);
    app_switcher_app_widget_set_focused(instance, true);

    self->focused_index = index;

    app_switcher_app_widget_preview(instance);
}

void app_switcher_app_widget_activate(AppSwitcherAppWidget *self) {
    WaylandService *wayland = wayland_service_get_global();

    AppSwitcherAppWidget *instance =
        find_instance_by_index(self, self->focused_index);

    WaylandWLRForeignTopLevel tp = {
        .toplevel = instance->wl_toplevel,
    };

    wayland_wlr_foreign_toplevel_activate(wayland, &tp);
}

GtkWidget *app_switcher_app_widget_get_widget(AppSwitcherAppWidget *self) {
    return GTK_WIDGET(self->container);
}

gchar *app_switcher_app_widget_get_app_id(AppSwitcherAppWidget *self) {
    return self->app_id;
}
