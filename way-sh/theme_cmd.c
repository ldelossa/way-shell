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
        "Commands:\n"
        "\tdark 		- set dark theme\n"
        "\tdown 		- set light theme\n");
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

cmd_tree_node_t *theme_cmd() {
	cmd_tree_node_add_child(&theme_root, &theme_dark_cmd);
	cmd_tree_node_add_child(&theme_root, &theme_light_cmd);

	return &theme_root;
}
