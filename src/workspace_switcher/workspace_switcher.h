#pragma once

#include <adwaita.h>

#include "workspace_switcher_workspace_widget.h"

G_BEGIN_DECLS

struct _WorkspaceSwitcher;
#define WORKSPACE_SWITCHER_TYPE workspace_switcher_get_type()
G_DECLARE_FINAL_TYPE(WorkspaceSwitcher, workspace_switcher, Workspace, Switcher,
                     GObject);

struct _WorkspaceModel;
#define WORKSPACE_MODEL_TYPE workspace_model_get_type()
G_DECLARE_FINAL_TYPE(WorkspaceModel, workspace_model, Workspace, Model,
                     GObject);

G_END_DECLS

void workspace_switcher_activate(AdwApplication *app, gpointer user_data);

WorkspaceSwitcher *workspace_switcher_get_global();

void workspace_switcher_show(WorkspaceSwitcher *self);

void workspace_switcher_hide(WorkspaceSwitcher *self);

void workspace_switcher_toggle(WorkspaceSwitcher *self);
