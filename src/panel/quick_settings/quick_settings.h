#pragma once

#include <adwaita.h>

#include "quick_settings_mediator.h"

G_BEGIN_DECLS

struct _QuickSettings;
#define QUICK_SETTINGS_TYPE quick_settings_get_type()
G_DECLARE_FINAL_TYPE(QuickSettings, quick_settings, QS, QUICK_SETTINGS,
                     GObject);

G_END_DECLS

// Intializes and activates the QuickSettings subsystem.
// After this returns the QuickSettings window can be requested via a signal and
// the global QuickSettingsMediator can be retrieved.
void quick_settings_activate(AdwApplication *app, gpointer user_data);

// Returns the global QuickSettingsMediator.
// Must call `quick_settings_activate` or returns NULL.
QuickSettingsMediator *quick_settings_get_global_mediator();

// Called to reinitialize the widget without allocating a new one.
void quick_settings_reinitialize(QuickSettings *self);

// Opens the QuickSettings relative to the given Panel.
void quick_settings_set_visible(QuickSettings *qs, GdkMonitor *mon);

// Closes the QuickSettings relative to the given Panel.
// If already hidden this results in a no-op.
void quick_settings_set_hidden(QuickSettings *qs, GdkMonitor *mon);

// Toggles the QuickSettings window open or closed, determined by whether the
// widget is visible or hidden under the give Panel.
void quick_settings_toggle(QuickSettings *qs, GdkMonitor *mon);

// Shrink the QuickSettings UI to its original size, if possible.
void quick_settings_shrink(QuickSettings *qs);

GdkMonitor *quick_settings_get_monitor(QuickSettings *qs);
