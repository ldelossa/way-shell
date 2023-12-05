#pragma once

#include <adwaita.h>

#include "panel_mediator.h"

#define PANEL_MAX_MONITORS 10

G_BEGIN_DECLS

// The top-of-screen Panel which exists on each monitor of the current
// GdkSeat.
struct _Panel;
#define PANEL_TYPE panel_get_type()
G_DECLARE_FINAL_TYPE(Panel, panel, PANEL, PANEL, GObject);

G_END_DECLS

// Initializes and activates the Panel subsystem.
// After this returns a set of Panels will exist for each monitor on
//
// TODO: Make this returns a pointer to the PanelMediator, such that it can be
// passed to other initializing systems if desired.
void panel_activate(AdwApplication *app, gpointer user_data);

// Returns an instance to the global PanelMediator.
// If `panel_activate` was not called prior to this it will return NULL, since
// the panel subsystem is not initialized.
//
// If the caller will hold a referene to PanelMediator it should increase it's
// ref count and decrement it once finished.
PanelMediator *panel_get_global_panel_mediator();