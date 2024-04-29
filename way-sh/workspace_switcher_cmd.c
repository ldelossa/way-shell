
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../lib/cmd_tree/include/cmd_tree.h"
#include "../src/services/ipc_service/ipc_commands.h"
#include "commands.h"

static int workspace_switcher_exec(void *ctx, uint8_t argc, char **argv) {
    printf(
        "Summary:\n"
        "\tManipulate the WorkspaceSwitcher widget.\n"
        "Commands:\n"
        "\tshow   - show the WorkspaceSwitcher widget\n"
        "\thide   - hide the WorkspaceSwitcher widget\n"
        "\ttoggle - toggle the WorkspaceSwitcher widget");
    return 0;
};

cmd_tree_node_t workspace_switcher_root = {.name = "workspace-switcher",
                                     .exec = workspace_switcher_exec};

static int workspace_switcher_show_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCWorkspaceSwitcherShow msg = {.header.type = IPC_CMD_WORKSPACE_SWITCHER_SHOW};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCWorkspaceSwitcherShow");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t workspace_switcher_show_cmd = {.name = "show",
                                         .exec = workspace_switcher_show_exec};

static int workspace_switcher_hide_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCWorkspaceSwitcherHide msg = {.header.type = IPC_CMD_WORKSPACE_SWITCHER_HIDE};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCWorkspaceSwitcherHide");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t workspace_switcher_hide_cmd = {.name = "hide",
                                         .exec = workspace_switcher_hide_exec};

static int workspace_switcher_toggle_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCWorkspaceSwitcherToggle msg = {.header.type = IPC_CMD_WORKSPACE_SWITCHER_TOGGLE};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCWorkspaceSwitcherToggle");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t workspace_switcher_toggle_cmd = {.name = "toggle",
                                           .exec = workspace_switcher_toggle_exec};

cmd_tree_node_t *workspace_switcher_cmd() {
    cmd_tree_node_add_child(&workspace_switcher_root, &workspace_switcher_show_cmd);
    cmd_tree_node_add_child(&workspace_switcher_root, &workspace_switcher_hide_cmd);
    cmd_tree_node_add_child(&workspace_switcher_root, &workspace_switcher_toggle_cmd);

    return &workspace_switcher_root;
};
