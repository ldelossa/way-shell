#pragma once
#ifndef CMD_TREE_H
#define CMD_TREE_H
#include <stdint.h>

#define CMD_TREE_MAX_NAME 256

/**
 * Function pointer which implements a node's command.
 * @param ctx A pointer to application specific context.
 * @param argc The count of trailing arguments to the command.
 * @param argv The trailing arguments to the command.
 */
typedef int (*cmd_tree_node_exec)(void *ctx, uint8_t argc, char **argv);

/**
 * A node in the cmd_tree.
 */
typedef struct cmd_tree_node {
    // Pointer to the next sibling node
    struct cmd_tree_node *sibling;
    // Pointer to the first child node
    struct cmd_tree_node *child;
    // Function pointer to the node's command
    cmd_tree_node_exec exec;
    // Array of arguments for the command
    char **argv;
    // Number of arguments for the command
    uint8_t argc;
    // Name of the command
    char name[CMD_TREE_MAX_NAME];
} cmd_tree_node_t;

/**
 * Adds a child node to a given node in the cmd_tree.
 *
 * @param n The node to add a child to.
 * @param child The child node to add.
 *
 * @return 1 on success, -1 on failure.
 */
int cmd_tree_node_add_child(cmd_tree_node_t *n, cmd_tree_node_t *child);

/**
 * Searches for a command node in the cmd_tree starting from the given root
 * node.
 *
 * @param root The root node of the cmd_tree to search from.
 * @param argc The length of argv.
 * @param argv An array of string pointers of size @argc
 * @param cmd_node A pointer to a cmd_tree_node_t pointer that will be set to
 * the found command node.
 *
 * @return 1 if the command node was found and -1 if an error occurred.
 */
int cmd_tree_search(cmd_tree_node_t *root, int argc, char *argv[],
                    cmd_tree_node_t **cmd_node);

#endif  // CMD_TREE_H
