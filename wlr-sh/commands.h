#include "../lib/cmd_tree/include/cmd_tree.h"

#define IPC_SEND_MSG(wlr_ctx, msg)                                 \
    struct sockaddr *addr = NULL;                                  \
    struct sockaddr_un addr_un = {                                 \
        .sun_family = AF_UNIX,                                     \
        .sun_path = {0},                                           \
    };                                                             \
    strcpy(&addr_un.sun_path[0], wlr_ctx->server_socket_path);     \
    addr = (struct sockaddr *)&addr_un;                            \
    ret = sendto(wlr_ctx->client_sock, &msg, sizeof(msg), 0, addr, \
                 sizeof(addr_un))

#define IPC_RECV_MSG(wlr_ctx, addr, bool) \
    recvfrom(wlr_ctx->client_sock, bool, sizeof(bool), 0, addr, 0)

typedef struct _ctx {
    char *server_socket_path;
    int client_sock;
} wlr_sh_ctx;

// The root command node.
//
// It is exec'd when no command is provided to wlr-sh and provides a short
// description of wlr-sh's usage.
//
// Defined in ./root.c
extern cmd_tree_node_t root_cmd;

// The message_tray comand root.
//
// Subcommands off this node deal with showing or manipulating the Message Tray
// UI component.
cmd_tree_node_t *message_tray_cmd();

// The volume command root.
//
// Subcommands off this node deal with adjusting the default audio sink's
// volume.
cmd_tree_node_t *volume_cmd();