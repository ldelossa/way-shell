#pragma once

#include <adwaita.h>

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

MessageTray *message_tray_get_global();

// Opens the MessageTray on the current monitor.
void message_tray_set_visible(MessageTray *self);

// Closes the MessageTray.
void message_tray_set_hidden(MessageTray *self);

void message_tray_reinitialize(MessageTray *self);

void message_tray_shrink(MessageTray *self);

void message_tray_runtime_signals(MessageTray *self);

void message_tray_toggle(MessageTray *self);
