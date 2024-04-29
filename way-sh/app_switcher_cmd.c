
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../lib/cmd_tree/include/cmd_tree.h"
#include "../src/services/ipc_service/ipc_commands.h"
#include "commands.h"

static int app_switcher_exec(void *ctx, uint8_t argc, char **argv) {
    printf(
        "Summary:\n"
        "\tManipulate the AppSwitcher widget.\n"
        "Commands:\n"
        "\tshow   - show the AppSwitcher widget\n"
        "\thide   - hide the AppSwitcher widget\n"
        "\ttoggle - toggle the AppSwitcher widget");
    return 0;
};

cmd_tree_node_t app_switcher_root = {.name = "app-switcher",
                                     .exec = app_switcher_exec};

static int app_switcher_show_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCAppSwitcherShow msg = {.header.type = IPC_CMD_APP_SWITCHER_SHOW};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCAppSwitcherShow");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t app_switcher_show_cmd = {.name = "show",
                                         .exec = app_switcher_show_exec};

static int app_switcher_hide_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCAppSwitcherHide msg = {.header.type = IPC_CMD_APP_SWITCHER_HIDE};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCAppSwitcherHide");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t app_switcher_hide_cmd = {.name = "hide",
                                         .exec = app_switcher_hide_exec};

static int app_switcher_toggle_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCAppSwitcherToggle msg = {.header.type = IPC_CMD_APP_SWITCHER_TOGGLE};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCAppSwitcherToggle");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t app_switcher_toggle_cmd = {.name = "toggle",
                                           .exec = app_switcher_toggle_exec};

cmd_tree_node_t *app_switcher_cmd() {
    cmd_tree_node_add_child(&app_switcher_root, &app_switcher_show_cmd);
    cmd_tree_node_add_child(&app_switcher_root, &app_switcher_hide_cmd);
    cmd_tree_node_add_child(&app_switcher_root, &app_switcher_toggle_cmd);

    return &app_switcher_root;
};
