#include "indicator_bar.h"

#include <adwaita.h>

#include "../../services/status_notifier_service/status_notifier_service.h"
#include "../panel.h"
#include "indicator_widget.h"

struct _IndicatorBar {
    GObject parent_instance;
    GtkBox *container;
    GtkBox *list;
    Panel *panel;
    GHashTable *indicators;
};
G_DEFINE_TYPE(IndicatorBar, indicator_bar, G_TYPE_OBJECT);

static void on_status_notifier_item_added(StatusNotifierService *sn,
                                          GHashTable *items,
                                          StatusNotifierItem *sni,
                                          IndicatorBar *self);

static void on_status_notifier_item_removed(StatusNotifierService *sn,
                                            GHashTable *items,
                                            StatusNotifierItem *sni,
                                            IndicatorBar *self);

// stub out dispose, finalize, class_init, and init methods
static void indicator_bar_dispose(GObject *gobject) {
    IndicatorBar *self = INDICATOR_BAR(gobject);

    // kill signals
    StatusNotifierService *sn = status_notifier_service_get_global();
    g_signal_handlers_disconnect_by_func(sn, on_status_notifier_item_added,
                                         self);
    g_signal_handlers_disconnect_by_func(sn, on_status_notifier_item_removed,
                                         self);

    // unref all indicators
    GList *values = g_hash_table_get_values(self->indicators);
    for (GList *l = values; l; l = l->next) {
        IndicatorWidget *i = l->data;
        g_object_unref(i);
    }

    // Chain-up
    G_OBJECT_CLASS(indicator_bar_parent_class)->dispose(gobject);
};

static void indicator_bar_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(indicator_bar_parent_class)->finalize(gobject);
};

static void indicator_bar_class_init(IndicatorBarClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = indicator_bar_dispose;
    object_class->finalize = indicator_bar_finalize;
};

static void on_status_notifier_item_added(StatusNotifierService *sn,
                                          GHashTable *items,
                                          StatusNotifierItem *sni,
                                          IndicatorBar *self) {
    g_debug("indicator_bar.c:on_status_notifier_item_added");

    IndicatorWidget *w = g_object_new(INDICATOR_WIDGET_TYPE, NULL);
    indicator_widget_set_sni(w, sni);

    g_hash_table_insert(self->indicators, sni->bus_name, w);
    gtk_box_append(self->list, indicator_widget_get_widget(w));
}

static void on_status_notifier_item_removed(StatusNotifierService *sn,
                                            GHashTable *items,
                                            StatusNotifierItem *sni,
                                            IndicatorBar *self) {
    g_debug("indicator_bar.c:on_status_notifier_item_removed");

    IndicatorWidget *i = g_hash_table_lookup(self->indicators, sni->bus_name);
    if (!i) return;

    GtkWidget *w = indicator_widget_get_widget(i);
    gtk_box_remove(self->list, w);

    g_hash_table_remove(self->indicators, sni->bus_name);
}

static void indicator_bar_init_layout(IndicatorBar *self) {
    g_debug("indicator_bar.c:indicator_bar_init_layout() called.");
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->container), "panel-indicator-bar");

    self->list = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->list),
                             "panel-indicator-bar-list");

    gtk_box_append(self->container, GTK_WIDGET(self->list));

    StatusNotifierService *sn = status_notifier_service_get_global();
    g_signal_connect(sn, "status-notifier-item-added",
                     G_CALLBACK(on_status_notifier_item_added), self);
    g_signal_connect(sn, "status-notifier-item-removed",
                     G_CALLBACK(on_status_notifier_item_removed), self);
}

static void indicator_bar_init(IndicatorBar *self) {
    g_debug("indicator_bar.c:indicator_bar_init() called.");
    self->indicators = g_hash_table_new(g_str_hash, g_str_equal);
    indicator_bar_init_layout(self);
}

GtkWidget *indicator_bar_get_widget(IndicatorBar *self) {
    return GTK_WIDGET(self->container);
}

void indicator_bar_set_panel(IndicatorBar *self, Panel *panel) {
    self->panel = panel;
}
