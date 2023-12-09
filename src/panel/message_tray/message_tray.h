#pragma once

#include <adwaita.h>

#include "message_tray_mediator.h"

G_BEGIN_DECLS

// The top-of-screen Panel which exists on each monitor of the current
// GdkSeat.
struct _MessageTray;
#define MESSAGE_TRAY_TYPE message_tray_get_type()
G_DECLARE_FINAL_TYPE(MessageTray, message_tray, MESSAGE_TRAY, TRAY, GObject);

G_END_DECLS

// Intializes and activates the MessageTray subsystem.
// After this returns the MessageTray window can be requested via a signal and
// the global MessageTrayMediator can be retrieved.
void message_tray_activate(AdwApplication *app, gpointer user_data);

// Returns the global MessageTrayMediator.
// Must call `message_tray_activate` or returns NULL.
MessageTrayMediator *message_tray_get_global_mediator();

// Opens the MessageTray relative to the given Panel.
void message_tray_set_visible(MessageTray *tray, Panel *panel);

// Closes the MessageTray relative to the given Panel.
// If already hidden this results in a no-op.
void message_tray_set_hidden(MessageTray *tray, Panel *panel);

Panel *message_tray_get_panel(MessageTray *tray);

// Toggles the MessageTray window open or closed, determined by whether the
// widget is visible or hidden under the give Panel.
void message_tray_toggle(MessageTray *tray, Panel *panel);