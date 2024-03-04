#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../lib/cmd_tree/include/cmd_tree.h"
#include "../src/services/ipc_service/ipc_commands.h"
#include "commands.h"

static int theme_exec(void *ctx, uint8_t argc, char **argv) {
    printf(
        "Summary:\n"
        "\tAdjust aspects of Way-Shell's theme.\n"
        "\n"
        "\tThe dump commands will write the respective theme to\n"
        "\t$HOME/.config/way-shell/way-shell-{light|dark}.css for editing.\n"
		"\n"
        "\tToggling the theme (light-to-dark or vice versa) will show the \n"
        "\tresults of any edits.\n"
        "Commands:\n"
        "\tdark 		- set dark theme\n"
        "\tlight 		- set light theme\n"
        "\tdump-dark 	- dump dark theme\n"
        "\tdump-light 	- dump light theme\n");
    return 0;
};

cmd_tree_node_t theme_root = {.name = "theme", .exec = theme_exec};

static int theme_dark_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCThemeDark msg = {.header.type = IPC_CMD_THEME_DARK};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCThemeDark");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t theme_dark_cmd = {.name = "dark", .exec = theme_dark_exec};

static int theme_light_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCThemeLight msg = {.header.type = IPC_CMD_THEME_LIGHT};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCThemeLight");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t theme_light_cmd = {.name = "light", .exec = theme_light_exec};

static int theme_dump_light_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCDumpLightTheme msg = {.header.type = IPC_CMD_DUMP_LIGHT_THEME};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCDumpLightTheme");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t theme_dump_light_cmd = {.name = "dump-light",
                                        .exec = theme_dump_light_exec};

static int theme_dump_dark_exec(void *ctx, uint8_t argc, char **argv) {
    int ret = 0;
    way_sh_ctx *way_ctx = ctx;

    IPCDumpDarkTheme msg = {.header.type = IPC_CMD_DUMP_DARK_THEME};

    IPC_SEND_MSG(way_ctx, msg);

    if (ret == -1) {
        perror("[Error] Failed to send IPCDumpDarkTheme");
        return -1;
    }

    bool response = false;
    IPC_RECV_MSG(way_ctx, addr, &response);

    return response;
};

cmd_tree_node_t theme_dump_dark_cmd = {.name = "dump-dark",
                                       .exec = theme_dump_dark_exec};

cmd_tree_node_t *theme_cmd() {
    cmd_tree_node_add_child(&theme_root, &theme_dark_cmd);
    cmd_tree_node_add_child(&theme_root, &theme_light_cmd);
    cmd_tree_node_add_child(&theme_root, &theme_dump_light_cmd);
    cmd_tree_node_add_child(&theme_root, &theme_dump_dark_cmd);

    return &theme_root;
}
