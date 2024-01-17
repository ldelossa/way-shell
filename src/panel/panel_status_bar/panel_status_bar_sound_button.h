#include <adwaita.h>

G_BEGIN_DECLS

struct _PanelStatusBarSoundButton;
#define PANEL_STATUS_BAR_SOUND_BUTTON_TYPE \
    panel_status_bar_sound_button_get_type()
G_DECLARE_FINAL_TYPE(PanelStatusBarSoundButton, panel_status_bar_sound_button,
                     PANEL, STATUS_BAR_SOUND_BUTTON, GObject);

G_END_DECLS

GtkWidget *panel_status_bar_sound_button_get_widget(
    PanelStatusBarSoundButton *self);