#include "include/cmd_tree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int cmd_tree_node_add_sibling(cmd_tree_node_t *n, cmd_tree_node_t *sibling) {
    if (n == NULL || sibling == NULL) {
        return -1;
    }
    if (!n->sibling) {
        n->sibling = sibling;
        return 1;
    }
    n = n->sibling;
    for (;;) {
        if (n->sibling == NULL) break;
        n = n->sibling;
    }
    n->sibling = sibling;
    return 1;
}

int cmd_tree_node_add_child(cmd_tree_node_t *n, cmd_tree_node_t *child) {
    if (n == NULL || child == NULL) {
        return -1;
    }
    if (!n->child) {
        n->child = child;
        return 1;
    }
    return cmd_tree_node_add_sibling(n->child, child);
}

int cmd_tree_search_assign_args(cmd_tree_node_t *root, int i, int argc,
                                char **argv) {

    root->argv = argv + i;
    root->argc = argc - i;
    return 1;
}

// recursively search for nodes which matches the latter most token in argv.
// we enter here only when argc > 0.
void cmd_tree_search_recur(cmd_tree_node_t *cur, int i, int argc, char *argv[],
                           cmd_tree_node_t **node) {
    // out of argv bounds, backtrack.
    if (i == argc) return;

    char *token = argv[i];

    cmd_tree_node_t *child = cur->child;

    while (child) {
        if (strcmp(child->name, token) == 0) {
            *node = child;
			// assign all trailing arguments to the child node.
            cmd_tree_search_assign_args(child, i + 1, argc, argv);
            cmd_tree_search_recur(child, i + 1, argc, argv, node);
            break;
        }
        child = child->sibling;
    }

    return;
}

int cmd_tree_search(cmd_tree_node_t *root, int argc, char *argv[],
                    cmd_tree_node_t **cmd_node) {
    if (!root) {
        return -1;
    }

	*cmd_node = root;

	// if there's no arguments just return root
    if (argc == 0) {
		root->argc = 0;
        return 1;
    }

    cmd_tree_search_recur(root, 0, argc, argv, cmd_node);
    return 1;
}
