#include "./activities_app_widget.h"

#include <adwaita.h>
#include <gio/gio.h>
#include <sys/wait.h>

#include "./activities.h"
#include "glib.h"

typedef struct _ActivitiesAppWidget {
    GObject parent_instance;
    GAppInfo *app_info;
    GtkBox *container;
    GtkImage *icon;
    GtkLabel *display_name;
    GtkButton *button;
    GtkBox *button_contents;
    guint parent_index;
} ActivitiesAppWidget;
G_DEFINE_TYPE(ActivitiesAppWidget, activities_app_widget, G_TYPE_OBJECT);

// fork off a new process in a new session group making it independent of
// our app.
//
// then, launch the desired application and kill the forked process.
// this orphans the launched process and the kernel reparents it under PID 1.
static void launch_with_fork(GAppInfo *app_info) {
    pid_t pid = fork();
    if (pid == -1) {
        g_warning("Failed to fork: %s", strerror(errno));
        return;
    }

    if (pid == 0) {
        GError *error = NULL;
        if (setsid() == -1) {
            g_warning("Failed to setsid: %s", strerror(errno));
            exit(1);
        }
        // launch the app
        g_app_info_launch(app_info, NULL, NULL, &error);
        if (error) {
            g_warning("Failed to launch app: %s", error->message);
            exit(1);
        }
        exit(0);
    }
}

// stub out dispose, finalize, class_init and init methods.
static void activities_app_widget_dispose(GObject *object) {
    G_OBJECT_CLASS(activities_app_widget_parent_class)->dispose(object);
}

static void activities_app_widget_finalize(GObject *object) {
    G_OBJECT_CLASS(activities_app_widget_parent_class)->finalize(object);
}

static void activities_app_widget_class_init(ActivitiesAppWidgetClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = activities_app_widget_dispose;
    object_class->finalize = activities_app_widget_finalize;
}

static void launch_app_on_click(GtkButton *button, ActivitiesAppWidget *self) {
    GError *error = NULL;
    GAppInfo *app_info = activities_app_widget_get_app_info(self);
    if (!app_info) {
        return;
    }

    const char *id = g_app_info_get_id(app_info);
    // Gnome apps seem to not open when forked, perform a normal launch.
    if (g_str_has_prefix(id, "org.gnome")) {
        g_app_info_launch(app_info, NULL, NULL, &error);
        if (error) {
            g_warning("Failed to launch app: %s", error->message);
        }
    } else {
        launch_with_fork(app_info);
    }

    // close Activities if opened
    Activities *activities = activities_get_global();
    activities_hide(activities);
}

static void on_container_destroyed(GtkWidget *widget,
                                   ActivitiesAppWidget *self) {
    g_clear_object(&self->app_info);
    g_object_unref(self);
}

static void activities_app_widget_layout(ActivitiesAppWidget *self) {
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->container),
                             "activities-app-widget");
    gtk_widget_set_halign(GTK_WIDGET(self->container), GTK_ALIGN_CENTER);
    gtk_widget_set_valign(GTK_WIDGET(self->container), GTK_ALIGN_CENTER);
    g_object_set_data(G_OBJECT(self->container), "app-widget", self);

    g_signal_connect(self->container, "destroy",
                     G_CALLBACK(on_container_destroyed), self);

    self->button = GTK_BUTTON(gtk_button_new());
    gtk_widget_set_size_request(GTK_WIDGET(self->button), 192, 192);
    g_signal_connect(self->button, "clicked", G_CALLBACK(launch_app_on_click),
                     self);

    self->button_contents = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_halign(GTK_WIDGET(self->button_contents), GTK_ALIGN_CENTER);
    gtk_widget_set_valign(GTK_WIDGET(self->button_contents), GTK_ALIGN_CENTER);

    self->icon = GTK_IMAGE(gtk_image_new());
    gtk_widget_add_css_class(GTK_WIDGET(self->icon),
                             "activities-app-widget-icon");

    self->display_name = GTK_LABEL(gtk_label_new(""));
    gtk_widget_add_css_class(GTK_WIDGET(self->display_name),
                             "activities-app-widget-display-name");

    gtk_box_append(self->button_contents, GTK_WIDGET(self->icon));
    gtk_box_append(self->button_contents, GTK_WIDGET(self->display_name));
    gtk_button_set_child(self->button, GTK_WIDGET(self->button_contents));
    gtk_box_append(self->container, GTK_WIDGET(self->button));
}

static void activities_app_widget_init(ActivitiesAppWidget *self) {
    activities_app_widget_layout(self);
}

void activities_app_widget_set_app_info(ActivitiesAppWidget *self,
                                        GAppInfo *app_info) {
    g_return_if_fail(self != NULL && G_IS_APP_INFO(app_info));

    g_object_ref(app_info);
    g_clear_object(&self->app_info);
    self->app_info = app_info;

    const char *display_name = g_app_info_get_display_name(app_info);
    gtk_label_set_text(GTK_LABEL(self->display_name),
                       display_name ? display_name : "");

    GIcon *icon = g_app_info_get_icon(app_info);
    // check and handle GFileIcon, set self->icon to a GtkImage
    if (G_IS_FILE_ICON(icon)) {
        g_debug("G_FILE_ICON");
    }
    // check and handle if GThemedIcon
    else if (G_IS_THEMED_ICON(icon)) {
        GdkDisplay *display = gdk_display_get_default();
        GtkIconTheme *theme = gtk_icon_theme_get_for_display(display);
        GtkIconPaintable *paintable = gtk_icon_theme_lookup_by_gicon(
            theme, icon, 78, 1, GTK_TEXT_DIR_RTL, 0);
        gtk_image_set_from_paintable(self->icon, GDK_PAINTABLE(paintable));
        gtk_image_set_pixel_size(self->icon, 78);
    }
    // check and handle if GLoadableIcon
    else if (G_IS_LOADABLE_ICON(icon)) {
        g_debug("G_LOADABLE_ICON");
    }
}

GAppInfo *activities_app_widget_get_app_info(ActivitiesAppWidget *self) {
    return self->app_info;
}

void activities_app_widget_simulate_click(ActivitiesAppWidget *self) {
    launch_app_on_click(self->button, self);
}

GtkWidget *activities_app_widget(ActivitiesAppWidget *self) {
    return GTK_WIDGET(self->container);
}

ActivitiesAppWidget *activities_app_widget_from_widget(GtkWidget *widget) {
    g_return_val_if_fail(widget != NULL, NULL);
    return g_object_get_data(G_OBJECT(widget), "app-widget");
}
