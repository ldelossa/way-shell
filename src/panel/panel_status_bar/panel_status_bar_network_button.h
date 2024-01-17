#include <adwaita.h>

G_BEGIN_DECLS

struct _PanelStatusBarNetworkButton;
#define PANEL_STATUS_BAR_NETWORK_BUTTON_TYPE \
    panel_status_bar_network_button_get_type()
G_DECLARE_FINAL_TYPE(PanelStatusBarNetworkButton,
                     panel_status_bar_network_button, PANEL,
                     STATUS_BAR_NETWORK_BUTTON, GObject);

G_END_DECLS

GtkWidget *panel_status_bar_network_button_get_widget(
    PanelStatusBarNetworkButton *self);
