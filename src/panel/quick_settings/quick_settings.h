#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

struct _QuickSettings;
#define QUICK_SETTINGS_TYPE quick_settings_get_type()
G_DECLARE_FINAL_TYPE(QuickSettings, quick_settings, QS, QUICK_SETTINGS,
                     GObject);

G_END_DECLS

// Intializes and activates the QuickSettings subsystem.
// After this returns the QuickSettings window can be requested via a signal and
void quick_settings_activate(AdwApplication *app, gpointer user_data);

// Called to reinitialize the widget without allocating a new one.
void quick_settings_reinitialize(QuickSettings *self);

// Opens the QuickSettings relative to the given Panel.
void quick_settings_set_visible(QuickSettings *qs);

// Closes the QuickSettings relative to the given Panel.
// If already hidden this results in a no-op.
void quick_settings_set_hidden(QuickSettings *qs);

// Toggles the QuickSettings window open or closed, determined by whether the
// widget is visible or hidden under the give Panel.
void quick_settings_toggle(QuickSettings *qs);

// Shrink the QuickSettings UI to its original size, if possible.
void quick_settings_shrink(QuickSettings *qs);

void quick_settings_set_focused(QuickSettings *qs, gboolean focus);

gboolean quick_settings_is_visible(QuickSettings *qs);

QuickSettings *quick_settings_get_global();

void quick_settings_toggle(QuickSettings *qs);
