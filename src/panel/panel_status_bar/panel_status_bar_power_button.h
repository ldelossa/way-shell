#include <adwaita.h>

G_BEGIN_DECLS

struct _PanelStatusBarPowerButton;
#define PANEL_STATUS_BAR_POWER_BUTTON_TYPE \
    panel_status_bar_power_button_get_type()
G_DECLARE_FINAL_TYPE(PanelStatusBarPowerButton, panel_status_bar_power_button,
                     PANEL, STATUS_BAR_POWER_BUTTON, GObject);

G_END_DECLS

GtkWidget *panel_status_bar_power_button_get_widget(PanelStatusBarPowerButton *self);