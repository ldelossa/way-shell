#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

struct _PanelWorkspacesBar;
#define PANEL_WORKSPACES_BAR_TYPE panel_workspaces_bar_get_type()
G_DECLARE_FINAL_TYPE(PanelWorkspacesBar, panel_workspaces_bar, PANEL,
                     WORKSPACES_BAR, GObject);

G_END_DECLS

typedef struct _Panel Panel;

GtkWidget *panel_workspaces_bar_get_widget(PanelWorkspacesBar *self);

void panel_workspaces_bar_set_panel(PanelWorkspacesBar *self, Panel *panel);