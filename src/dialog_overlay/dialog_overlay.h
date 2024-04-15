#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

// Shows a full-output overlay with a dialog in the middle.
// The user can either select "confirm" or "cancel".
// When "confirm" is selected the GCallback provided to "dialog_overlay_present"
// is called.
struct _DialogOverlay;
#define DIALOG_OVERLAY_TYPE dialog_overlay_get_type()
G_DECLARE_FINAL_TYPE(DialogOverlay, dialog_overlay, Dialog, Overlay, GObject);

G_END_DECLS

void dialog_overlay_reinitialize(DialogOverlay *self);

void dialog_overlay_activate(AdwApplication *app, gpointer user_data);

DialogOverlay *dialog_overlay_get_global();

void dialog_overlay_present(DialogOverlay *self, gchar *heading, gchar *body,
                            GCallback cb);
