#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../lib/cmd_tree/include/cmd_tree.h"
#include "../src/services/ipc_service/ipc_commands.h"
#include "commands.h"

static int rename_switcher_exec(void *ctx, uint8_t argc, char **argv) {
    printf(
        "Summary:\n"
        "\tManipulate the RenameSwitcher widget.\n"
        "Commands:\n"
        "\tshow   - show the RenameSwitcher widget\n"
        "\thide   - hide the RenameSwitcher widget\n"
        "\ttoggle - toggle the RenameSwitcher widget");
    return 0;
};

cmd_tree_node_t rename_switcher_root = {.name = "rename-switcher",
                                     .exec = rename_switcher_exec};

static int rename_switcher_show_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCRenameSwitcherShow msg = {.header.type = IPC_CMD_RENAME_SWITCHER_SHOW};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCRenameSwitcherShow");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t rename_switcher_show_cmd = {.name = "show",
                                         .exec = rename_switcher_show_exec};

static int rename_switcher_hide_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCRenameSwitcherHide msg = {.header.type = IPC_CMD_RENAME_SWITCHER_HIDE};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCRenameSwitcherHide");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t rename_switcher_hide_cmd = {.name = "hide",
                                         .exec = rename_switcher_hide_exec};

static int rename_switcher_toggle_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCRenameSwitcherToggle msg = {.header.type = IPC_CMD_RENAME_SWITCHER_TOGGLE};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCRenameSwitcherToggle");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t rename_switcher_toggle_cmd = {.name = "toggle",
                                           .exec = rename_switcher_toggle_exec};

cmd_tree_node_t *rename_switcher_cmd() {
    cmd_tree_node_add_child(&rename_switcher_root, &rename_switcher_show_cmd);
    cmd_tree_node_add_child(&rename_switcher_root, &rename_switcher_hide_cmd);
    cmd_tree_node_add_child(&rename_switcher_root, &rename_switcher_toggle_cmd);

    return &rename_switcher_root;
};
