#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../lib/cmd_tree/include/cmd_tree.h"
#include "../src/services/ipc_service/ipc_commands.h"
#include "commands.h"

static int activities_exec(void *ctx, uint8_t argc, char **argv) {
    printf(
        "Summary:\n"
        "\tManipulate the Activities widget.\n"
        "Commands:\n"
        "\tshow   - show the Activities widget\n"
        "\thide   - hide the Activities widget\n"
        "\ttoggle - toggle the Activities widget");
    return 0;
};

cmd_tree_node_t activities_root = {.name = "activities",
                                   .exec = activities_exec};

static int activities_show_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCActivitiesShow msg = {.header.type = IPC_CMD_ACTIVITIES_SHOW};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCActivitiesShow");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t activities_show_cmd = {.name = "show",
                                       .exec = activities_show_exec};

static int activities_hide_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCActivitiesHide msg = {.header.type = IPC_CMD_ACTIVITIES_HIDE};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCActivitiesHide");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t activities_hide_cmd = {.name = "hide",
                                       .exec = activities_hide_exec};

static int activities_toggle_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCActivitiesToggle msg = {.header.type = IPC_CMD_ACTIVITIES_TOGGLE};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCActivitiesToggle");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t activities_toggle_cmd = {.name = "toggle",
                                         .exec = activities_toggle_exec};

cmd_tree_node_t *activities_cmd() {
    cmd_tree_node_add_child(&activities_root, &activities_show_cmd);
    cmd_tree_node_add_child(&activities_root, &activities_hide_cmd);
    cmd_tree_node_add_child(&activities_root, &activities_toggle_cmd);

    return &activities_root;
};
