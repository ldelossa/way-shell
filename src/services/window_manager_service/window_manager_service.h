#pragma once

#include <adwaita.h>

typedef enum WMWorkspaceEventType {
    WMWORKSPACE_EVENT_CREATED,
    WMWORKSPACE_EVENT_DESTROYED,
    WMWORKSPACE_EVENT_FOCUSED,
    WMWORKSPACE_EVENT_MOVED,
    WMWORKSPACE_EVENT_RENAMED,
    WMWORKSPACE_EVENT_URGENT,
    WMWORKSPACE_EVENT_RELOAD,
    WMWORKSPACE_EVENT_LEN,
} WMWorkspaceEventType;

extern char *WMWorkspaceEventStringTbl[];

static __always_inline char *window_manager_service_event_to_string(
    WMWorkspaceEventType type) {
    if (type >= WMWORKSPACE_EVENT_LEN) {
        return NULL;
    }

    return WMWorkspaceEventStringTbl[type];
}

static __always_inline WMWorkspaceEventType
window_manager_service_string_to_event(char *str) {
    for (int i = 0; i < WMWORKSPACE_EVENT_LEN; i++) {
        if (g_strcmp0(WMWorkspaceEventStringTbl[i], str) == 0) {
            return i;
        }
    }

    return -1;
}

typedef struct _WMWorkspace {
    gchar *name;
    gchar *output;
    guint32 id;
    gint32 num;
    gboolean urgent;
    gboolean focused;
    gboolean visible;
    gboolean empty;
} WMWorkspace;

typedef struct _WMWorkspaceEvent {
    WMWorkspaceEventType type;
    WMWorkspace workspace;
} WMWorkspaceEvent;

typedef struct _WMOutputEvent {
} WMOutputEvent;

typedef struct _WMOutput {
    gchar *name;
    gchar *make;
    gchar *model;
    gchar *serial;
    gchar *current_workspace;
} WMOutput;

// WinowManager is a virtual function table which abstracts an underlying
// window manager implementation.
//
// Each implementation should define a function which returns a WindowManager
// with its method's set to internal function pointers.
typedef struct _WindowManager WindowManager;
typedef GPtrArray *(*wm_get_workspaces_func)(WindowManager *self);

typedef GPtrArray *(*wm_get_outputs_func)(WindowManager *self);

typedef int (*wm_focus_workspace_func)(WindowManager *self, WMWorkspace *ws);

typedef int (*wm_current_ws_to_output_func)(WindowManager *self, WMOutput *o);

typedef int (*wm_current_app_to_workspace_func)(WindowManager *self,
                                                WMWorkspace *ws);

typedef void (*wm_on_workspaces_changed)(WindowManager *self,
                                         GPtrArray *workspaces, void *data);
typedef void (*wm_on_outputs_changed)(WindowManager *self, GPtrArray *outputs,
                                      void *data);

typedef guint (*wm_register_on_workspaces_changed)(WindowManager *self,
                                                   wm_on_workspaces_changed cb,
                                                   void *data);

typedef guint (*wm_unregister_on_workspaces_changed)(
    WindowManager *self, wm_on_workspaces_changed cb, void *data);

typedef guint (*wm_register_on_outputs_changed)(WindowManager *self,
                                                wm_on_outputs_changed cb,
                                                void *data);

typedef guint (*wm_unregister_on_outputs_changed)(WindowManager *self,
                                                  wm_on_outputs_changed cb,
                                                  void *data);

typedef struct _WindowManager {
    // private struct which implements the WindowManager interface, interface
    // methods below can cast this to their internal type.
    void *private;
    // Provide a list of currently available workspaces
    wm_get_workspaces_func get_workspaces;
    // Provide a list of currently available outputs
    wm_get_outputs_func get_outputs;
    // Focus the provided workspace
    wm_focus_workspace_func focus_workspace;
    // Move the current workspace to the specified output
    wm_current_ws_to_output_func current_ws_to_output;
    // Move the current app to the specified workspace
    wm_current_app_to_workspace_func current_app_to_workspace;
    // register a callback when workspaces has changed.
    // returns the GObject signal ID on success.
    wm_register_on_workspaces_changed register_on_workspaces_changed;
    // unregister a callback when workspaces has changed.
    wm_unregister_on_workspaces_changed unregister_on_workspaces_changed;
    // register a callback when outputs has changed.
    // returns the GObject signal ID on success.
    wm_register_on_outputs_changed register_on_outputs_changed;
    // unregister a callback when outputs has changed.
    wm_unregister_on_outputs_changed unregister_on_outputs_changed;
} WindowManager;

// Initialize the window manager service
int window_manager_service_init();

// Obtain the global window manager service, a call to
// `window_manager_service_init` must be made before this.
WindowManager *window_manager_service_get_global();
