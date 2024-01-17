#pragma once

#include <adwaita.h>

#include "panel.h"

G_BEGIN_DECLS

struct _PanelClock;
#define PANEL_CLOCK_TYPE panel_clock_get_type()
G_DECLARE_FINAL_TYPE(PanelClock, panel_clock, PANEL, CLOCK, GObject);

G_END_DECLS

GtkWidget *panel_clock_get_widget(PanelClock *self);

GtkButton *panel_clock_get_button(PanelClock *self);

void panel_clock_set_panel(PanelClock *self, Panel *panel);

void panel_clock_set_toggled(PanelClock *self, gboolean toggled);

gboolean panel_clock_get_toggled(PanelClock *self);

void panel_clock_reinitialize(PanelClock *self);
