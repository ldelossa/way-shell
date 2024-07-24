#include <adwaita.h>

G_BEGIN_DECLS

struct _PanelStatusBarNightLightButton;
#define PANEL_STATUS_BAR_NIGHTLIGHT_BUTTON_TYPE \
    panel_status_bar_nightlight_button_get_type()
G_DECLARE_FINAL_TYPE(PanelStatusBarNightLightButton, panel_status_bar_nightlight_button,
                     PANEL, STATUS_BAR_NIGHTLIGHT_BUTTON, GObject);

G_END_DECLS

GtkWidget *panel_status_bar_nightlight_button_get_widget(PanelStatusBarNightLightButton *self);
