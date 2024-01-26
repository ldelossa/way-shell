#include <adwaita.h>

G_BEGIN_DECLS

struct _QuickSettingsScales;
#define QUICK_SETTINGS_SCALES_TYPE quick_settings_scales_get_type()
G_DECLARE_FINAL_TYPE(QuickSettingsScales, quick_settings_scales, QUICK_SETTINGS,
                     SCALES, GObject);

G_END_DECLS

void quick_settings_scales_reinitialize(QuickSettingsScales *self);

GtkWidget *quick_settings_scales_get_widget(QuickSettingsScales *self);

void quick_settings_scales_disable_audio_scales(
    QuickSettingsScales *self, gboolean disable);