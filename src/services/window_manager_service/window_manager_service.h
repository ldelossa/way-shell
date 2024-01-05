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
