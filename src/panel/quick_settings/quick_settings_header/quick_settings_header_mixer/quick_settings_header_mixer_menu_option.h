#include <adwaita.h>

#include "../../../../services/wireplumber_service.h"

G_BEGIN_DECLS

struct _QuickSettingsHeaderMixerMenuOption;
#define QUICK_SETTINGS_HEADER_MIXER_MENU_OPTION_TYPE \
    quick_settings_header_mixer_menu_option_get_type()
G_DECLARE_FINAL_TYPE(QuickSettingsHeaderMixerMenuOption,
                     quick_settings_header_mixer_menu_option, QUICK_SETTINGS,
                     HEADER_MIXER_MENU_OPTION, GObject);

G_END_DECLS

GtkWidget *quick_settings_header_mixer_menu_option_get_widget(
    QuickSettingsHeaderMixerMenuOption *self);

void quick_settings_header_mixer_menu_option_set_node(
    QuickSettingsHeaderMixerMenuOption *self,
    WirePlumberServiceNodeHeader *node);

WirePlumberServiceNodeHeader *quick_settings_header_mixer_menu_option_get_node(
    QuickSettingsHeaderMixerMenuOption *self);
