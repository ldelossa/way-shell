#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../lib/cmd_tree/include/cmd_tree.h"
#include "../src/services/ipc_service/ipc_commands.h"
#include "commands.h"

static int message_tray_exec(void *ctx, uint8_t argc, char **argv) {
    printf(
        "Summary:\n"
        "\tManipulate the Message Tray component.\n"
        "Commands:\n"
        "\topen - open the message tray component on the primary monitor");
    return 0;
};

// The root command for wlr-sh and is displayed when no other arguments are
// provided to the CLI.
//
// A short help blurb is presented along with a list of all available root level
// commands.
cmd_tree_node_t message_tray_root = {.name = "message-tray",
                                     .exec = message_tray_exec};

static int message_tray_open_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    wlr_sh_ctx *wlr_ctx = ctx;

    IPCMessageTrayOpen msg = {.header.type = IPC_CMD_MESSAGE_TRAY_OPEN};

    IPC_SEND_MSG(wlr_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCMessageTrayOpen");
        return -1;
    }

    return 0;
};

cmd_tree_node_t message_tray_open_cmd = {.name = "open",
                                         .exec = message_tray_open_exec};

cmd_tree_node_t *message_tray_cmd() {
    cmd_tree_node_add_child(&message_tray_root, &message_tray_open_cmd);
    return &message_tray_root;
};