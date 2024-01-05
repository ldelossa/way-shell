#include <adwaita.h>

G_BEGIN_DECLS

struct _QuickSettingsHeader;
#define QUICK_SETTINGS_HEADER_TYPE quick_settings_header_get_type()
G_DECLARE_FINAL_TYPE(QuickSettingsHeader, quick_settings_header, QUICK_SETTINGS,
                     HEADER, GObject);

G_END_DECLS

void quick_settings_header_reinitialize(QuickSettingsHeader *self);

GtkWidget *quick_settings_header_get_widget(QuickSettingsHeader *self);
