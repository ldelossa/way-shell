#include <adwaita.h>

G_BEGIN_DECLS

struct _PanelStatusBarVPNButton;
#define PANEL_STATUS_BAR_VPN_BUTTON_TYPE \
    panel_status_bar_vpn_button_get_type()
G_DECLARE_FINAL_TYPE(PanelStatusBarVPNButton, panel_status_bar_vpn_button,
                     PANEL, STATUS_BAR_VPN_BUTTON, GObject);

G_END_DECLS

GtkWidget *panel_status_bar_vpn_button_get_widget(PanelStatusBarVPNButton *self);
