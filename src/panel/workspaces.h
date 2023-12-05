#include <gtk/gtk.h>
#include <glib.h>
#include <stdint.h>
#include "gtk/gtkcssprovider.h"

enum workspaces_errors {
	workspaces_err_not_found,
};

// workspace_t defines a workspace that has been created by the compositor.
typedef struct workspace {
	GString name;
	GString output;
	uint32_t id;
	GtkButton *button;
} workspace_t;

// implements the workspaces widget within the Gnomeland status bar.
typedef struct workspaces_widget_t {
	GList workspace_list;
	GtkBox *box;
} workspaces_widget_t;

// initialize a workspaces_widget_t
uint8_t workspaces_init(workspaces_widget_t *w);

// free a workspaces_t and all resources belonging to it.
// only safe to call when all workspace_t resources are not in use.
uint8_t free_workspaces(workspace_t *w);

// index a new workspace.
// @w: a workspaces_t 
// @ws: a workspace_t to add to workspaces_t
uint8_t add_workspace(workspaces_widget_t *w, workspace_t *ws);

// removes a workspace.
// @w: a workspaces_t 
// @ws: a workspace_t to remove to workspaces_t
uint8_t remove_workspace(workspaces_widget_t *w, workspace_t *ws);

// initialize a workspace_t
uint8_t workspace_init(workspace_t *ws);

// free a workspace_t and all resources belonging to it.
uint8_t free_workspace(workspace_t *ws);
