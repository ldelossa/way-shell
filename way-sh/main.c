#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "../lib/cmd_tree/include/cmd_tree.h"
#include "./commands.h"

#define SOCK_NAME "/way-shell.sock"
#define CLIENT_SOCK_PREFIX "way-shell-"

int client_socket_create(way_sh_ctx *ctx) {
    struct sockaddr_un addr = {.sun_family = AF_UNIX, .sun_path = {0}};
    char *p = &addr.sun_path[1];
    int r = rand();

    memcpy(p, CLIENT_SOCK_PREFIX, sizeof(CLIENT_SOCK_PREFIX) - 1);
    p += sizeof(CLIENT_SOCK_PREFIX) - 1;
    memcpy(p, &r, sizeof(r));

    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd <= 0) {
        printf("[Error] Failed to create socket\n");
        return -1;
    }

    if (bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) != 0) {
        printf("[Error] Failed to bind socket\n");
        return -1;
    }

    ctx->client_sock = fd;

    return 0;
}

static void build_command_tree() {
    cmd_tree_node_t *message_tray = message_tray_cmd();
    cmd_tree_node_t *volume = volume_cmd();
    cmd_tree_node_t *brightness = brightness_cmd();
	cmd_tree_node_t *theme = theme_cmd();

    cmd_tree_node_add_child(&root_cmd, message_tray);
    cmd_tree_node_add_child(&root_cmd, volume);
    cmd_tree_node_add_child(&root_cmd, brightness);
	cmd_tree_node_add_child(&root_cmd, theme);
}

int main(int argc, char **argv) {
    way_sh_ctx ctx = {0};
    int ret = 0;
    char socket_path[256] = {0};
    struct stat statsbuf = {0};
    cmd_tree_node_t *cmd = {0};

    // check if XDG_RUNTIME_DIR is set
    char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!xdg_runtime_dir) {
        printf(
            "[Error] XDG_RUNTIME_DIR env variable must be set so we can find "
            "way-shell's IPC socket\n");
        return -1;
    }

    strcpy(socket_path, xdg_runtime_dir);
    strcat(socket_path, SOCK_NAME);

    // ensure socket exists
    if (stat(socket_path, &statsbuf) != 0) {
        printf("[Error] IPC socket does not exist at %s\n", socket_path);
        return -1;
    }
    ctx.server_socket_path = socket_path;

    // ensure file is indeed a socket
    if (!S_ISSOCK(statsbuf.st_mode)) {
        printf("[Error] IPC socket path is not a socket: %s\n", socket_path);
        return -1;
    }

    client_socket_create(&ctx);

    build_command_tree();

    // adjust argc and argv one past binary name.
    // garbage in argv is fine, cmd_tree api handles this.
    if (cmd_tree_search(&root_cmd, argc - 1, argv + 1, &cmd) != 1) {
        printf("Failed to find command");
        return -1;
    }

    if (!cmd) {
        printf("Failed to find command");
        return -1;
    }

    ret = cmd->exec(&ctx, cmd->argc, cmd->argv);

    close(ctx.client_sock);

    // flip boolean values for exit codes
    if (ret) {
        return 0;
    }

    return 1;
}
