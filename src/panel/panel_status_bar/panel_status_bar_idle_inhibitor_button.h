#include <adwaita.h>

G_BEGIN_DECLS

struct _PanelStatusBarIdleInhibitorButton;
#define PANEL_STATUS_BAR_IDLE_INHIBITOR_BUTTON_TYPE \
    panel_status_bar_idle_inhibitor_button_get_type()
G_DECLARE_FINAL_TYPE(PanelStatusBarIdleInhibitorButton, panel_status_bar_idle_inhibitor_button,
                     PANEL, STATUS_BAR_IDLE_INHIBITOR_BUTTON, GObject);

G_END_DECLS

GtkWidget *panel_status_bar_idle_inhibitor_button_get_widget(PanelStatusBarIdleInhibitorButton *self);
