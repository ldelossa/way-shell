
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../lib/cmd_tree/include/cmd_tree.h"
#include "../src/services/ipc_service/ipc_commands.h"
#include "commands.h"

static int output_switcher_exec(void *ctx, uint8_t argc, char **argv) {
    printf(
        "Summary:\n"
        "\tManipulate the OutputSwitcher widget.\n"
        "Commands:\n"
        "\tshow   - show the OutputSwitcher widget\n"
        "\thide   - hide the OutputSwitcher widget\n"
        "\ttoggle - toggle the OutputSwitcher widget");
    return 0;
};

cmd_tree_node_t output_switcher_root = {.name = "output-switcher",
                                     .exec = output_switcher_exec};

static int output_switcher_show_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCOutputSwitcherShow msg = {.header.type = IPC_CMD_OUTPUT_SWITCHER_SHOW};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCOutputSwitcherShow");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t output_switcher_show_cmd = {.name = "show",
                                         .exec = output_switcher_show_exec};

static int output_switcher_hide_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCOutputSwitcherHide msg = {.header.type = IPC_CMD_OUTPUT_SWITCHER_HIDE};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCOutputSwitcherHide");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t output_switcher_hide_cmd = {.name = "hide",
                                         .exec = output_switcher_hide_exec};

static int output_switcher_toggle_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCOutputSwitcherToggle msg = {.header.type = IPC_CMD_OUTPUT_SWITCHER_TOGGLE};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCOutputSwitcherToggle");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t output_switcher_toggle_cmd = {.name = "toggle",
                                           .exec = output_switcher_toggle_exec};

cmd_tree_node_t *output_switcher_cmd() {
    cmd_tree_node_add_child(&output_switcher_root, &output_switcher_show_cmd);
    cmd_tree_node_add_child(&output_switcher_root, &output_switcher_hide_cmd);
    cmd_tree_node_add_child(&output_switcher_root, &output_switcher_toggle_cmd);

    return &output_switcher_root;
};
