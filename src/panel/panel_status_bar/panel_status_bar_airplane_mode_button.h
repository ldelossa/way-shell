#include <adwaita.h>

G_BEGIN_DECLS

struct _PanelStatusBarAirplaneModeButton;
#define PANEL_STATUS_BAR_AIRPLANE_MODE_BUTTON_TYPE \
    panel_status_bar_airplane_mode_button_get_type()
G_DECLARE_FINAL_TYPE(PanelStatusBarAirplaneModeButton, panel_status_bar_airplane_mode_button,
                     PANEL, STATUS_BAR_AIRPLANE_MODE_BUTTON, GObject);

G_END_DECLS

GtkWidget *panel_status_bar_airplane_mode_button_get_widget(PanelStatusBarAirplaneModeButton *self);
