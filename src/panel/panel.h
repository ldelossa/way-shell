#pragma once

#include <adwaita.h>

#include "panel_mediator.h"

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
PanelMediator *panel_get_global_mediator();

// Called when the provided panel influenced the MessageTray to become visible.
void panel_on_msg_tray_visible(Panel *panel);

// Called when the provided panel influenced the MessageTray to be hidden.
void panel_on_msg_tray_hidden(Panel *panel);

// Called when the provided panel influenced the QuickSettings to become visible.
void panel_on_qs_visible(Panel *panel);

// Called when the provided panel influenced the QuickSettings to be hidden.
void panel_on_qs_hidden(Panel *panel);

// Returns the monitor the given Panel is on.
GdkMonitor *panel_get_monitor(Panel *panel);

// Attaches and displays a panel to the given monitor
void panel_attach_to_monitor(Panel *panel, GdkMonitor *mon);

Panel *panel_get_from_monitor(GdkMonitor *monitor);

GHashTable *panel_get_all_panels();
