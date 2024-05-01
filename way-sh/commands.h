#include "../lib/cmd_tree/include/cmd_tree.h"

#define IPC_SEND_MSG(way_ctx, msg)                                 \
    struct sockaddr *addr = NULL;                                  \
    struct sockaddr_un addr_un = {                                 \
        .sun_family = AF_UNIX,                                     \
        .sun_path = {0},                                           \
    };                                                             \
    strcpy(&addr_un.sun_path[0], way_ctx->server_socket_path);     \
    addr = (struct sockaddr *)&addr_un;                            \
    ret = sendto(way_ctx->client_sock, &msg, sizeof(msg), 0, addr, \
                 sizeof(addr_un))

#define IPC_RECV_MSG(way_ctx, addr, bool) \
    recvfrom(way_ctx->client_sock, bool, sizeof(bool), 0, addr, 0)

typedef struct _ctx {
    char *server_socket_path;
    int client_sock;
} way_sh_ctx;

// The root command node.
//
// It is exec'd when no command is provided to way-sh and provides a short
// description of way-sh's usage.
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

// The Brightness command
//
// Subcommands off this node deal with adjusting the default display's
// brightness.
cmd_tree_node_t *brightness_cmd();

// The Theme command
//
// Subcommands off this node deal with adjusting Way-Shell' theme.
cmd_tree_node_t *theme_cmd();

// The Activities command
//
// Subcommands off this node deal with showing and hiding the Activities widget.
cmd_tree_node_t *activities_cmd();

// The App Switcher command
//
// Subcommands off this node deal with showing and hiding the App Switcher
// widget.
cmd_tree_node_t *app_switcher_cmd();

// The Workspace Switcher command
//
// Subcommands off this node deal with showing and hiding the Workspace Switcher
// widget.
cmd_tree_node_t *workspace_switcher_cmd();

// The Output Switcher command
//
// Subcommands off this node deal with showing and hiding the Output Switcher
// widget.
cmd_tree_node_t *output_switcher_cmd();
