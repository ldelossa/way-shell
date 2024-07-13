#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

struct _RenameSwitcher;
#define RENAME_SWITCHER_TYPE rename_switcher_get_type()
G_DECLARE_FINAL_TYPE(RenameSwitcher, rename_switcher, Rename, Switcher,
                     GObject);

G_END_DECLS

void rename_switcher_activate(AdwApplication *app, gpointer user_data);

RenameSwitcher *rename_switcher_get_global();

void rename_switcher_show(RenameSwitcher *self);

void rename_switcher_hide(RenameSwitcher *self);

void rename_switcher_toggle(RenameSwitcher *self);

