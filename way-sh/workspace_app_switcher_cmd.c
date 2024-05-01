
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../lib/cmd_tree/include/cmd_tree.h"
#include "../src/services/ipc_service/ipc_commands.h"
#include "commands.h"

static int workspace_app_switcher_exec(void *ctx, uint8_t argc, char **argv) {
    printf(
        "Summary:\n"
        "\tManipulate the WorkspaceAppSwitcher widget.\n"
        "Commands:\n"
        "\tshow   - show the WorkspaceAppSwitcher widget\n"
        "\thide   - hide the WorkspaceAppSwitcher widget\n"
        "\ttoggle - toggle the WorkspaceAppSwitcher widget");
    return 0;
};

cmd_tree_node_t workspace_app_switcher_root = {.name = "workspace-app-switcher",
                                     .exec = workspace_app_switcher_exec};

static int workspace_app_switcher_show_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCWorkspaceAppSwitcherShow msg = {.header.type = IPC_CMD_WORKSPACE_APP_SWITCHER_SHOW};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCWorkspaceAppSwitcherShow");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t workspace_app_switcher_show_cmd = {.name = "show",
                                         .exec = workspace_app_switcher_show_exec};

static int workspace_app_switcher_hide_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCWorkspaceAppSwitcherHide msg = {.header.type = IPC_CMD_WORKSPACE_APP_SWITCHER_HIDE};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCWorkspaceAppSwitcherHide");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t workspace_app_switcher_hide_cmd = {.name = "hide",
                                         .exec = workspace_app_switcher_hide_exec};

static int workspace_app_switcher_toggle_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCWorkspaceAppSwitcherToggle msg = {.header.type = IPC_CMD_WORKSPACE_APP_SWITCHER_TOGGLE};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCWorkspaceAppSwitcherToggle");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t workspace_app_switcher_toggle_cmd = {.name = "toggle",
                                           .exec = workspace_app_switcher_toggle_exec};

cmd_tree_node_t *workspace_app_switcher_cmd() {
    cmd_tree_node_add_child(&workspace_app_switcher_root, &workspace_app_switcher_show_cmd);
    cmd_tree_node_add_child(&workspace_app_switcher_root, &workspace_app_switcher_hide_cmd);
    cmd_tree_node_add_child(&workspace_app_switcher_root, &workspace_app_switcher_toggle_cmd);

    return &workspace_app_switcher_root;
};
