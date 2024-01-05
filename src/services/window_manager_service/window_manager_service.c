#include "window_manager_service.h"

char *WMWorkspaceEventStringTbl[WMWORKSPACE_EVENT_LEN] = {
    "CREATED", "DESTROYED", "FOCUSED", "MOVED", "RENAMED", "URGENT", "RELOAD",
};