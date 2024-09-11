
#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

struct _WorkspaceSwitcherWorkspaceWidget;
#define WORKSPACE_SWITCHER_WORKSPACE_WIDGET_TYPE \
    workspace_switcher_workspace_widget_get_type()
G_DECLARE_FINAL_TYPE(WorkspaceSwitcherWorkspaceWidget,
                     workspace_switcher_workspace_widget, WorkspaceSwitcher,
                     WorkspaceWidget, GObject);

G_END_DECLS

void workspace_switcher_workspace_widget_set_workspace_name(
    WorkspaceSwitcherWorkspaceWidget *self, const gchar *name);

GtkWidget *workspace_switcher_workspace_widget_get_widget(
    WorkspaceSwitcherWorkspaceWidget *self);
