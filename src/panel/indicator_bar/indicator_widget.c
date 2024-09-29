#include "indicator_widget.h"

#include <adwaita.h>

#include "../../services/status_notifier_service/status_notifier_item_dbus.h"
#include "../../services/status_notifier_service/status_notifier_service.h"

struct _IndicatorWidget {
    GObject parent_instance;
    GtkBox *container;
    GtkBox *box;
    GtkImage *icon;
    GtkButton *button;
    GtkPopoverMenu *menu;
    StatusNotifierItem *sni;
};
G_DEFINE_TYPE(IndicatorWidget, indicator_widget, G_TYPE_OBJECT);

static void on_sni_menu_updated(StatusNotifierService *s,
                                StatusNotifierItem *sni, IndicatorWidget *self);
static void on_new_icon(DbusItemV0Gen *item, IndicatorWidget *self);
// stub out dispose, finalize, class_init, and init methods
static void indicator_widget_dispose(GObject *gobject) {
    IndicatorWidget *self = INDICATOR_WIDGET(gobject);
    // Chain-up
    G_OBJECT_CLASS(indicator_widget_parent_class)->dispose(gobject);
    // kill signals on sni->proxy
    g_signal_handlers_disconnect_by_func(self->sni->proxy, on_new_icon, self);

    StatusNotifierService *s = status_notifier_service_get_global();
    g_signal_handlers_disconnect_by_func(s, on_sni_menu_updated, self);
};

static void indicator_widget_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(indicator_widget_parent_class)->finalize(gobject);
};

static void indicator_widget_class_init(IndicatorWidgetClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = indicator_widget_dispose;
    object_class->finalize = indicator_widget_finalize;
};

static void on_button_clicked(GtkButton *button, IndicatorWidget *self) {
    GError *error = NULL;
    dbus_item_v0_gen_call_activate_sync(
        status_notifier_item_get_proxy(self->sni), 0, 0, NULL, &error);
    if (error) {
        g_error_free(error);
    }
}

static void indicator_widget_init_layout(IndicatorWidget *self) {
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->container),
                        "panel-indicator-bar-widget");

    self->box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->box),
                             "panel-indicator-bar-widget-box");

    self->icon = GTK_IMAGE(gtk_image_new_from_icon_name("image-missing"));

    self->button = GTK_BUTTON(gtk_button_new());
    gtk_button_set_child(self->button, GTK_WIDGET(self->icon));

    // wire it up
    gtk_box_append(self->box, GTK_WIDGET(self->button));
    gtk_box_append(self->container, GTK_WIDGET(self->box));
}

static void indicator_widget_init(IndicatorWidget *self) {
    indicator_widget_init_layout(self);
}

GtkWidget *indicator_widget_get_widget(IndicatorWidget *self) {
    return GTK_WIDGET(self->container);
}

static void on_button_clicked_with_menu(GtkButton *button,
                                        IndicatorWidget *self) {
    gtk_popover_popup(GTK_POPOVER(self->menu));
    status_notifier_item_about_to_show(self->sni, 0);
}

static void indicator_widget_set_icon(IndicatorWidget *self) {
    const gchar *icon_name = status_notifier_item_get_icon_name(self->sni);

    if (icon_name && strlen(icon_name) > 0) {
        gtk_image_set_from_icon_name(self->icon, icon_name);
    } else {
        GdkPixbuf *pix = status_notifier_item_get_icon_pixmap(self->sni);
        if (pix) {
            GdkTexture *t = gdk_texture_new_for_pixbuf(pix);
            gtk_image_set_from_paintable(self->icon, GDK_PAINTABLE(t));
        } else {
            gtk_image_set_from_icon_name(self->icon, "image-missing");
        }
    }
}

static void on_new_icon(DbusItemV0Gen *sni, IndicatorWidget *self) {
    g_debug("indicator_widget.c:on_new_icon() called");
    indicator_widget_set_icon(self);
}

static void on_sni_menu_updated(StatusNotifierService *s,
                                StatusNotifierItem *sni,
                                IndicatorWidget *self) {
    g_debug("indicator_widget.c:on_sni_menu_updated() called");
    if (sni != self->sni) return;
    if (!sni->menu_model) return;
    gtk_popover_menu_set_menu_model(self->menu, G_MENU_MODEL(sni->menu_model));
}

void indicator_widget_set_sni(IndicatorWidget *self, StatusNotifierItem *sni) {
    self->sni = sni;

    indicator_widget_set_icon(self);
    g_signal_connect(sni->proxy, "new-icon", G_CALLBACK(on_new_icon), self);
    g_signal_connect(sni->proxy, "new-attention-icon", G_CALLBACK(on_new_icon),
                     self);
    g_signal_connect(sni->proxy, "new-overlay-icon", G_CALLBACK(on_new_icon),
                     self);

    if (sni->menu_model)
        g_signal_connect(self->button, "clicked",
                         G_CALLBACK(on_button_clicked_with_menu), self);
    else
        g_signal_connect(self->button, "clicked", G_CALLBACK(on_button_clicked),
                         self);

    if (!sni->menu_model) return;

    self->menu = GTK_POPOVER_MENU(gtk_popover_menu_new_from_model_full(
        G_MENU_MODEL(sni->menu_model), GTK_POPOVER_MENU_NESTED));

    gtk_widget_set_parent(GTK_WIDGET(self->menu), GTK_WIDGET(self->button));

    // insert the action group the SNI provides at the prefix
    // SNI_GACTION_PREFIX. GMenuItems will send actions there.
    gtk_widget_insert_action_group(GTK_WIDGET(self->menu), SNI_GACTION_PREFIX,
                                   self->sni->action_group);

    StatusNotifierService *s = status_notifier_service_get_global();
    g_signal_connect(s, "status-notifier-item-menu-updated",
                     G_CALLBACK(on_sni_menu_updated), self);
}

StatusNotifierItem *indicator_widget_get_sni(IndicatorWidget *self) {
    return self->sni;
}
