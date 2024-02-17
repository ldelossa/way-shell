#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

// A IPC Service which directly interfaces with the `wlr-sh` CLI.
//
// The service talks over a DGRAM Unix socket located at
// $XDG_RUNTIME_DIR/wlr-shell.sock
struct _IPCService;
#define IPC_SERVICE_TYPE ipc_service_get_type()
G_DECLARE_FINAL_TYPE(IPCService, ipc_service, IPC, SERVICE, GObject);

G_END_DECLS

int ipc_service_global_init(void);

// Get the global ipc service
// Will return NULL if `ipc_service_global_init` has not been called.
IPCService *ipc_service_get_global();
