#pragma once

#include <adwaita.h>

#include "../panel.h"

G_BEGIN_DECLS

struct _IndicatorBar;
#define INDICATOR_BAR_TYPE indicator_bar_get_type()
G_DECLARE_FINAL_TYPE(IndicatorBar, indicator_bar, INDICATOR, BAR, GObject);

G_END_DECLS

// Sets the owning panel
void indicator_bar_set_panel(IndicatorBar *self, Panel *panel);

// Sets the QS button's toggle state.
// Effects the css applied to the button.
void indicator_bar_set_toggled(IndicatorBar *self, gboolean toggled);

GtkWidget *indicator_bar_get_widget(IndicatorBar *self);
