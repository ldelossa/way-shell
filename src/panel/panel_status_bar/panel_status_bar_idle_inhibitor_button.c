#include "panel_status_bar_idle_inhibitor_button.h"

#include <adwaita.h>

#include "../../services/logind_service/logind_service.h"

struct _PanelStatusBarIdleInhibitorButton {
    GObject parent_instance;
    GtkImage *icon;
};
G_DEFINE_TYPE(PanelStatusBarIdleInhibitorButton,
              panel_status_bar_idle_inhibitor_button, G_TYPE_OBJECT);

static void on_idle_inhibitor_changed(LogindService *logind_service,
                                      gboolean inhibited,
                                      PanelStatusBarIdleInhibitorButton *self) {
    g_debug(
        "panel_status_bar_idle_inhibitor_button.c: "
        "on_idle_inhibitor_changed(): "
        "idle inhibitor changed: %d",
        inhibited);

    if (inhibited)
        gtk_widget_set_visible(GTK_WIDGET(self->icon), true);
    else
        gtk_widget_set_visible(GTK_WIDGET(self->icon), false);
};

// stub out dispose, finalize, class_init, and init methods
static void panel_status_bar_idle_inhibitor_button_dispose(GObject *gobject) {
    PanelStatusBarIdleInhibitorButton *self =
        PANEL_STATUS_BAR_IDLE_INHIBITOR_BUTTON(gobject);

    // kill signals
    LogindService *logind = logind_service_get_global();
    g_signal_handlers_disconnect_by_func(logind, on_idle_inhibitor_changed,
                                         self);

    // Chain-up
    G_OBJECT_CLASS(panel_status_bar_idle_inhibitor_button_parent_class)
        ->dispose(gobject);
};

static void panel_status_bar_idle_inhibitor_button_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(panel_status_bar_idle_inhibitor_button_parent_class)
        ->finalize(gobject);
};

static void panel_status_bar_idle_inhibitor_button_class_init(
    PanelStatusBarIdleInhibitorButtonClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = panel_status_bar_idle_inhibitor_button_dispose;
    object_class->finalize = panel_status_bar_idle_inhibitor_button_finalize;
};

static void panel_status_bar_idle_inhibitor_button_init_layout(
    PanelStatusBarIdleInhibitorButton *self) {
    self->icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("radio-mixed-symbolic"));

    LogindService *logind = logind_service_get_global();
    if (!logind) {
        gtk_widget_set_visible(GTK_WIDGET(self->icon), false);
        return;
    }

    // get initial inhibited status
    gboolean inhibited =
        logind_service_get_idle_inhibit(logind_service_get_global());

    // if not inhibited set image visible false
    if (!inhibited) gtk_widget_set_visible(GTK_WIDGET(self->icon), false);

    // connect idle inhibitor signal on logind
    g_signal_connect(logind, "idle-inhibitor-changed",
                     G_CALLBACK(on_idle_inhibitor_changed), self);
};

static void panel_status_bar_idle_inhibitor_button_init(
    PanelStatusBarIdleInhibitorButton *self) {
    panel_status_bar_idle_inhibitor_button_init_layout(self);
}

GtkWidget *panel_status_bar_idle_inhibitor_button_get_widget(
    PanelStatusBarIdleInhibitorButton *self) {
    return GTK_WIDGET(self->icon);
}
