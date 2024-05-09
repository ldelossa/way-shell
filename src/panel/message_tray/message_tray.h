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

// Opens the MessageTray relative to the given Monitor.
void message_tray_set_visible(MessageTray *self, GdkMonitor *monitor);

// Closes the MessageTray relative to the given Montor.
// If already hidden this results in a no-op.
void message_tray_set_hidden(MessageTray *self, GdkMonitor *monitor);

GdkMonitor *message_tray_get_monitor(MessageTray *self);

void message_tray_reinitialize(MessageTray *self);

void message_tray_shrink(MessageTray *self);
