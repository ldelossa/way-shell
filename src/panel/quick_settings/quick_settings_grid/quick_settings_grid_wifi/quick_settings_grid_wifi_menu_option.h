#include <NetworkManager.h>
#include <adwaita.h>

G_BEGIN_DECLS

struct _QuickSettingsGridWifiMenuOption;
#define QUICK_SETTINGS_GRID_WIFI_MENU_OPTION_TYPE \
    quick_settings_grid_wifi_menu_option_get_type()
G_DECLARE_FINAL_TYPE(QuickSettingsGridWifiMenuOption,
                     quick_settings_grid_wifi_menu_option, QUICK_SETTINGS,
                     GRID_WIFI_MENU_OPTION, GObject);

G_END_DECLS

typedef struct _QuickSettingsGridWifiMenu QuickSettingsGridWifiMenu;

GtkWidget *quick_settings_grid_wifi_menu_option_get_widget(
    QuickSettingsGridWifiMenuOption *self);

void quick_settings_grid_wifi_menu_option_set_ap(
    QuickSettingsGridWifiMenuOption *self, QuickSettingsGridWifiMenu *menu,
    NMDeviceWifi *dev, NMAccessPoint *ap);

// ap getter
NMAccessPoint *quick_settings_grid_wifi_menu_option_get_ap(
    QuickSettingsGridWifiMenuOption *self);

// device getter
NMDeviceWifi *quick_settings_grid_wifi_menu_option_get_device(
    QuickSettingsGridWifiMenuOption *self);

