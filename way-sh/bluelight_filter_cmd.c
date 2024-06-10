#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../lib/cmd_tree/include/cmd_tree.h"
#include "../src/services/ipc_service/ipc_commands.h"
#include "commands.h"

static int bluelight_filter_exec(void *ctx, uint8_t argc, char **argv) {
    printf(
        "Summary:\n"
        "\tAdjust the bluelight (Night Light) filter.\n"
        "Commands:\n"
        "\tenable - turn on the bluelight filter\n"
        "\tdisable - turn off the bluelight filter");
    return 0;
};

// The root command for way-sh and is displayed when no other arguments are
// provided to the CLI.
//
// A short help blurb is presented along with a list of all available root level
// commands.
cmd_tree_node_t bluelight_filter_root = {.name = "bluelight-filter",
                                         .exec = bluelight_filter_exec};

static int bluelight_filter_enable_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCBlueLightFilterEnable msg = {.header.type =
                                        IPC_CMD_BLUELIGHT_FILTER_ENABLE};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCBlueLightFilterEnable");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t bluelight_filter_enable_cmd = {
    .name = "enable", .exec = bluelight_filter_enable_exec};

static int bluelight_filter_disable_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCBlueLightFilterDisable msg = {.header.type =
                                         IPC_CMD_BLUELIGHT_FILTER_DISABLE};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCBlueLightFilterDisable");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t bluelight_filter_disable_cmd = {
    .name = "disable", .exec = bluelight_filter_disable_exec};

cmd_tree_node_t *bluelight_filter_cmd() {
    cmd_tree_node_add_child(&bluelight_filter_root,
                            &bluelight_filter_enable_cmd);
    cmd_tree_node_add_child(&bluelight_filter_root,
                            &bluelight_filter_disable_cmd);
    return &bluelight_filter_root;
};
