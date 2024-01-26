#include <adwaita.h>

G_BEGIN_DECLS

struct _QuickSettingsHeaderMixer;
#define QUICK_SETTINGS_HEADER_MIXER_TYPE quick_settings_header_mixer_get_type()
G_DECLARE_FINAL_TYPE(QuickSettingsHeaderMixer, quick_settings_header_mixer,
                     QUICK_SETTINGS, HEADER_MIXER, GObject);

G_END_DECLS

void quick_settings_header_mixer_reinitialize(QuickSettingsHeaderMixer *self);

GtkButton *quick_settings_header_mixer_get_mixer_button(
    QuickSettingsHeaderMixer *self);

GtkWidget *quick_settings_header_mixer_get_widget(
    QuickSettingsHeaderMixer *self);

GtkWidget *quick_settings_header_mixer_get_menu_widget(
    QuickSettingsHeaderMixer *self);