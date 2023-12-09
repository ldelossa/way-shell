#include <adwaita.h>

#include "panel.h"

enum IconTypes {
    // left to right order
    // variable, only present when enabled.
    bluetooth,
    microphone,
    // fixed and always present
    network,
    sound,
    power,
    icons_n,
};

G_BEGIN_DECLS

struct _QuickSettingsButton;
#define QUICK_SETTINGS_BUTTON_TYPE quick_settings_button_get_type()
G_DECLARE_FINAL_TYPE(QuickSettingsButton, quick_settings_button, QUICK_SETTINGS,
                     BUTTON, GObject);

GtkImage *quick_settings_button_get_icon(QuickSettingsButton *self,
                                         enum IconTypes pos);

GtkButton *quick_settings_button_get_button(QuickSettingsButton *self);

GtkBox *quick_settings_button_get_widget(QuickSettingsButton *self);

// Setting an icon to an empty string will make it invisible.
int qs_button_set_icon(QuickSettingsButton *b, enum IconTypes pos,
                       const char *name);

// Sets the owning panel
void quick_settings_button_set_panel(QuickSettingsButton *self, Panel *panel);

// Sets the QS button's toggle state.
// Effects the css applied to the button.
void quick_settings_button_set_toggled(QuickSettingsButton *qs, gboolean toggled);

// Returns whether the QS button is toggled or not.
gboolean quick_settings_get_toggled(QuickSettingsButton *self);

G_END_DECLS