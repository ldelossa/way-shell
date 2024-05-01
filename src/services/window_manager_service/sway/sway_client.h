#pragma once

#include <adwaita.h>

#include "../window_manager_service.h"

typedef enum SWAY_CLIENT_ERR {
    SWAY_CLIENT_OK,
    SWAY_CLIENT_ERR_SOCKET_PATH_TOO_BIG,
    SWAY_CLIENT_ERR_SOCKET_CREATE_FAIL,
    SWAY_CLIENT_ERR_SOCKET_CONNECT_FAIL,
    SWAY_CLIENT_ERR_SOCKET_READ,
    SWAY_CLIENT_ERR_SOCKET_WRITE,
    SWAY_CLIENT_ERR_PROTOCOL_BAD_MAGIC
} SWAY_CLIENT_ERR;

// A Sway client extremely tuned for our application's needs.
// It is assumed that the IPC socket is managed by GLib's event loop which will
// call a callback function when the socket is ready for reading.
//
// Memory management rules for payloads:
// `sway_client_ipc_rcv` returns msg.payload malloc'd internally.
// `sway_client_*_resp` methods read msg.payload and frees it.
// `sway_client_ipc_send` frees msg.payload on successful send.
//
// Therefore, caller has limited responsibility of freeing payloads in
// the happy path. Caller should only have to malloc and fill payload buffers
// when it wants to make an IPC request.

static const char sway_client_ipc_magic[] = {'i', '3', '-', 'i', 'p', 'c'};
#define SWAY_CLIENT_IPC_MAGIC_SIZE sizeof(sway_client_ipc_magic)
#define SWAY_CLIENT_IPC_HEADER_SIZE (SWAY_CLIENT_IPC_MAGIC_SIZE + 8)

// A representation of an IPC message to or from Sway's IPC server.
typedef struct _sway_client_ipc_msg {
    gchar *payload;
    // order here is significant since we can memcpy right from
    // i3-ipc header to these fields.
    guint32 size;
    guint32 type;
} sway_client_ipc_msg;

gchar *sway_client_find_socket_path();

SWAY_CLIENT_ERR sway_client_ipc_connect(gchar *socket_path);

// Maps a Sway IPC event into a WMWorkspaceEventType.
WMWorkspaceEventType sway_client_event_map(char *event);

// Send a message to the connected IPC socket.
// Caller is responsible for freeing msg->payload once it's no longer needed
// with g_free if a payload exists.
int sway_client_ipc_send(int socket_fd, sway_client_ipc_msg *msg);

// Receives a message from the connected IPC socket.
// Caller is responsible for freeing msg->payload once it's no longer needed
// with g_free.
//
// Always check that msg->payload != nil incase a reply contained no payload.
int sway_client_ipc_recv(int socket_fd, sway_client_ipc_msg *msg);

// Commands //

// Send a 'get_workspaces' request.
// Msg details are handled internally.
int sway_client_ipc_get_workspaces_req(int socket_fd);

// Parses a response for a 'get_workspaces' request.
// Returns a GPtrArray of WMWorkspace structures with ref of 1.
// Unref GPtrArray when finished.
GPtrArray *sway_client_ipc_get_workspaces_resp(sway_client_ipc_msg *msg);

// Send a 'get_outputs' request.
// Msg details are handled internally.
int sway_client_ipc_get_outputs_req(int socket_fd);

// Parses a response for a 'get_outputs' request.
// Returns a GPtrArray of WMOutput structures with ref of 1.
// Unref GPtrArray when finished.
GPtrArray *sway_client_ipc_get_outputs_resp(sway_client_ipc_msg *msg);

// Subscribes specific Sway events.
int sway_client_ipc_subscribe_req(int socket_fd, int events[], guint len);

// Handles the response from a Subscribe request.
gboolean sway_client_ipc_subscribe_resp(sway_client_ipc_msg *msg);

// Focus the provided workspace.
int sway_client_ipc_focus_workspace(int socket_fd, WMWorkspace *ws);

// Move the given workspace to the provided output.
int sway_client_ipc_move_ws_to_output(int socket_fd, gchar *output);

// Events //

// Parses a workspace event and returns a WMWorkspaceEvent
//
// Its expected that the caller maintains an inventory of existing WMWorkspace
// sructures and can compare the IDs between the returned event and an existing
// workspace.
//
// While a WMWorkspaceEvent will embed a full WMWorkspace its important to only
// update existing WMWorkspace fields based on the event type, as other fields
// in event.workspace may be nonsense.
WMWorkspaceEvent *sway_client_ipc_event_workspace_resp(
    sway_client_ipc_msg *msg);

