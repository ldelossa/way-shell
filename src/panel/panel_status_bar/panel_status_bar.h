#pragma once

#include <adwaita.h>

#include "../panel.h"

G_BEGIN_DECLS

struct _PanelStatusBar;
#define PANEL_STATUS_BAR_TYPE panel_status_bar_get_type()
G_DECLARE_FINAL_TYPE(PanelStatusBar, panel_status_bar, PANEL, STATUS_BAR,
                     GObject);

G_END_DECLS

// Sets the owning panel
void panel_status_bar_set_panel(PanelStatusBar *self, Panel *panel);

// Sets the QS button's toggle state.
// Effects the css applied to the button.
void panel_status_bar_set_toggled(PanelStatusBar *self, gboolean toggled);

// Returns whether the QS button is toggled or not.
gboolean quick_settings_get_toggled(PanelStatusBar *self);

GtkBox *panel_status_bar_get_widget(PanelStatusBar *self);
