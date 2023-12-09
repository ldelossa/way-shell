#include <adwaita.h>

#include "panel.h"
#include "panel_clock.h"
#include "../services/clock_service.h"

struct _PanelClock {
    GObject parent_instance;
    Panel *panel;
    GtkBox *container;
    GtkButton *button;
    GtkLabel *label;
    gboolean toggled;

};
G_DEFINE_TYPE(PanelClock, panel_clock, G_TYPE_OBJECT)

// stub out dispose, finalize, class_init, and init methods
static void panel_clock_dispose(GObject *gobject) {
    PanelClock *self = PANEL_CLOCK(gobject);

    if (self->panel) {
        g_object_unref(self->panel);
    }

    // Chain-up
    G_OBJECT_CLASS(panel_clock_parent_class)->dispose(gobject);
};

static void panel_clock_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(panel_clock_parent_class)->finalize(gobject);
};

static void panel_clock_class_init(PanelClockClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = panel_clock_dispose;
    object_class->finalize = panel_clock_finalize;
};

static void on_tick(ClockService *cs, GDateTime *now, PanelClock *self) {
    gchar *date = g_date_time_format(now, "%b %d %I:%M");
    gtk_label_set_text(self->label, date);
    g_free(date);
}

static void panel_clock_init(PanelClock *self) {
    // set initial value before we wait for clock service
    GDateTime *now = g_date_time_new_now_local();
    gchar *date = g_date_time_format(now, "%b %d %I:%M");

    // setup signal to global clock service
    ClockService *cs = clock_service_get_global();
    g_signal_connect(cs, "tick", G_CALLBACK(on_tick), self);

    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->container), "panel-clock");
    self->button = GTK_BUTTON(gtk_button_new());
    gtk_widget_add_css_class(GTK_WIDGET(self->button), "panel-button");
    self->label = GTK_LABEL(gtk_label_new(date));
    gtk_button_set_child(self->button, GTK_WIDGET(self->label));
    gtk_box_append(self->container, GTK_WIDGET(self->button));
};

static void on_click(GtkWidget *w, Panel *panel) {
    PanelMediator *pm = panel_get_global_mediator();
    if (!pm)
        return;
    panel_mediator_emit_msg_tray_toggle_request(pm, panel);
}

void panel_clock_set_panel(PanelClock *self, Panel *panel) {
    g_signal_connect(self->button, "clicked", G_CALLBACK(on_click), panel);
    self->panel = g_object_ref(panel);
    self->panel = panel;
}

void panel_clock_set_toggled(PanelClock *self, gboolean toggled) {
    if (toggled) {
        gtk_widget_add_css_class(GTK_WIDGET(self->button), "panel-button-toggled");
    } else {
        gtk_widget_remove_css_class(GTK_WIDGET(self->button), "panel-button-toggled");
    }
    self->toggled = toggled;
};

gboolean panel_clock_get_toggled(PanelClock *self) {
    return self->toggled;
};

GtkWidget *panel_clock_get_widget(PanelClock *self) {
    return GTK_WIDGET(self->container);
};

GtkButton *panel_clock_get_button(PanelClock *self) {
    return self->button;
};
