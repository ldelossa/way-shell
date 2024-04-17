#include <stdio.h>

#include "../lib/cmd_tree/include/cmd_tree.h"

static int root_exec(void *ctx, uint8_t argc, char **argv) {
    printf(
        "Usage: \n"
        "\tway-sh COMMAND [SUBCOMMAND...] [ARGUMENTS]\n"
        "Commands: \n"
        "\tmessage-tray\n"
        "\tvolume\n"
        "\tbrightness\n"
        "\ttheme\n"
        "\tactivities\n"
        "\tapp-switcher\n"
	);
    return 0;
};

// The root command for way-sh and is displayed when no other arguments are
// provided to the CLI.
//
// A short help blurb is presented along with a list of all available root level
// commands.
cmd_tree_node_t root_cmd = {.exec = root_exec};
