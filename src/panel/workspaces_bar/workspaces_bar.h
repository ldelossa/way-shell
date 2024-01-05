#pragma once

#include <adwaita.h>

typedef struct _Panel Panel;

typedef struct _WorkspacesBar {
    Panel *panel;
    GtkBox *container;
    GtkScrolledWindow *scroll_win;
    GtkBox *list;
    GPtrArray *workspaces;
} WorkSpacesBar;

WorkSpacesBar *workspaces_bar_new(Panel *panel);

void workspaces_bar_free(WorkSpacesBar *workspaces);

GtkWidget *workspaces_bar_get_widget(WorkSpacesBar *self);