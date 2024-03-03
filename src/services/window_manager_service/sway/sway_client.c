#include "sway_client.h"

#include <adwaita.h>
#include <asm-generic/errno.h>
#include <errno.h>
#include <glob.h>
#include <json-glib/json-glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../../window_manager_service/window_manager_service.h"
#include "ipc.h"

WMWorkspaceEventType sway_client_event_map(char *event) {
    if (g_strcmp0(event, "init") == 0) return WMWORKSPACE_EVENT_CREATED;
    if (g_strcmp0(event, "empty") == 0) return WMWORKSPACE_EVENT_DESTROYED;
    if (g_strcmp0(event, "focus") == 0) return WMWORKSPACE_EVENT_FOCUSED;
    if (g_strcmp0(event, "move") == 0) return WMWORKSPACE_EVENT_MOVED;
    if (g_strcmp0(event, "rename") == 0) return WMWORKSPACE_EVENT_RENAMED;
    if (g_strcmp0(event, "urgent") == 0) return WMWORKSPACE_EVENT_URGENT;
    if (g_strcmp0(event, "reload") == 0) return WMWORKSPACE_EVENT_RELOAD;
    return -1;
}

gchar *sway_client_find_socket_glob() {
    glob_t res;
    char pattern[1024];
    char *xdg_runtime_dir = NULL;

    xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!xdg_runtime_dir) return NULL;

    snprintf(pattern, sizeof(pattern), "%s/sway-ipc.*.sock", xdg_runtime_dir);

    if (glob(pattern, 0, NULL, &res) != 0) return NULL;

    if (res.gl_pathc == 0) {
        globfree(&res);
        return NULL;
    }

    char *socket_path = g_strdup(res.gl_pathv[0]);
    globfree(&res);

    return socket_path;
}

gchar *sway_client_find_socket_path() {
    char *socket_path;

    g_debug("sway_client.c:sway_client_find_socket_path() called");

    socket_path = getenv("SWAYSOCK");

    if (!socket_path) socket_path = getenv("I3SOCK");

    // No env vars lets see we can find it xdg runtime dir
    if (!socket_path) socket_path = sway_client_find_socket_glob();
    if (socket_path) {
        return socket_path;
    }

    return g_strdup(socket_path);
}

SWAY_CLIENT_ERR sway_client_ipc_connect(gchar *socket_path) {
    int socket_fd = -1;

    g_debug("sway_client.c:sway_client_ipc_connect() called");

    if (!socket_path) return socket_fd;

    struct sockaddr_un addr = {0};

    if ((strlen(socket_path) + 1) > sizeof(addr.sun_path))
        return -SWAY_CLIENT_ERR_SOCKET_PATH_TOO_BIG;

    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, socket_path);

    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd == -1) return -SWAY_CLIENT_ERR_SOCKET_CREATE_FAIL;

    if (connect(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        return -SWAY_CLIENT_ERR_SOCKET_CONNECT_FAIL;

    return socket_fd;
}

static int socket_write(int socket_fd, uint8_t *buff, int size) {
    int n = size;
    int b = write(socket_fd, buff, n);
    for (; n > 0; b = write(socket_fd, buff, n)) {
        if (b < 0) {
            if (errno == EINTR) continue;
            if (errno) {
                g_critical(
                    "sway_client.c:sway_client_ipc_send() "
                    "failed to write to socket %d %s.",
                    errno, strerror(errno));
                return -SWAY_CLIENT_ERR_SOCKET_WRITE;
            }
        }
        n -= b;
        b += b;
    }
    return size;
}

static int socket_read(int socket_fd, uint8_t *buff, int size) {
    int n = size;
    int b = read(socket_fd, buff, n);
    for (; n > 0; b = read(socket_fd, buff, n)) {
        if (b < 0) {
            if (errno == EINTR) continue;
            if (errno) {
                g_critical(
                    "sway_client.c:sway_client_ipc_send() "
                    "failed to read from socket %d %s.",
                    errno, strerror(errno));
                return -SWAY_CLIENT_ERR_SOCKET_READ;
            }
        }
        n -= b;
        b += b;
    }
    return size;
}

int sway_client_ipc_send(int socket_fd, sway_client_ipc_msg *msg) {
    uint8_t buff[SWAY_CLIENT_IPC_HEADER_SIZE + msg->size];
    uint8_t *p = buff;
    int n = 0;

    g_debug("sway_client.c:sway_client_ipc_send() called");

    // copy in ipc magic
    for (int i = 0; i < SWAY_CLIENT_IPC_MAGIC_SIZE; i++) {
        *p = sway_client_ipc_magic[i];
        p++;
    }

    // copy in size and type
    memcpy(p, &msg->size, 8);
    p += 8;

    // copy in payload
    if (msg->size > 0 && msg->payload) memcpy(p, msg->payload, msg->size);

    // send off the buffer
    n = socket_write(socket_fd, buff, sizeof(buff));
    if (n < 0) return n;

    // we can free payload, caller shouldn't use it once its sent anyway.
    if (msg->payload) {
        g_free(msg->payload);
    }

    return 0;
}

int sway_client_ipc_recv(int socket_fd, sway_client_ipc_msg *msg) {
    uint8_t buff[SWAY_CLIENT_IPC_HEADER_SIZE] = {0};
    uint8_t *p = buff;
    int n = 0;

    g_debug("sway_client.c:sway_client_ipc_recv() called");

    // read in header
    n = socket_read(socket_fd, buff, sizeof(buff));
    if (n < 0) return n;

    // confirm i3-ipc magic
    for (int i = 0; i < SWAY_CLIENT_IPC_MAGIC_SIZE; i++) {
        if (*p != sway_client_ipc_magic[i])
            return -SWAY_CLIENT_ERR_PROTOCOL_BAD_MAGIC;
        p++;
    }

    // copy next 8 bytes to msg, will be in native endian
    memcpy(&msg->size, p, 8);

    // rest is payload, malloc a buffer for it, read in data, and return to
    // caller. caller is responsible for freeing this if a payload exists.
    if (msg->size == 0) {
        return 0;
    }

    msg->payload = g_malloc0(msg->size);
    n = socket_read(socket_fd, (uint8_t *)msg->payload, msg->size);
    if (n < 0) return n;

    g_debug(
        "sway_client.c:sway_client_ipc_recv() "
        "received ipc msg of type %u and size %u",
        msg->type, msg->size);

    return 0;
}

int sway_client_ipc_get_workspaces_req(int socket_fd) {
    sway_client_ipc_msg msg = {0};
    msg.type = IPC_GET_WORKSPACES;

    return sway_client_ipc_send(socket_fd, &msg);
}

int sway_client_parse_workspace_json(JsonObject *obj, WMWorkspace *ws) {
    JsonNode *node = NULL;

    // parse id -> ws.id
    node = json_object_get_member(obj, "id");
    if (!node) return -1;
    ws->id = json_node_get_int(node);

    // parse num -> ws.num
    node = json_object_get_member(obj, "num");
    if (!node)
        g_error(
            "sway_client.c:sway_client_parse_workspace() "
            "failed to read member 'num'.");
    ws->num = json_node_get_int(node);

    // get name
    node = json_object_get_member(obj, "name");
    if (!node)
        g_error(
            "sway_client.c:sway_client_parse_workspace() "
            "failed to read member 'name'.");
    ws->name = g_strdup(json_node_get_string(node));

    // parse urgent boolean
    node = json_object_get_member(obj, "urgent");
    if (!node)
        g_error(
            "sway_client.c:sway_client_parse_workspace() "
            "failed to read member 'urgent'.");
    ws->urgent = json_node_get_boolean(node);

    // parse output
    node = json_object_get_member(obj, "output");
    if (!node)
        g_error(
            "sway_client.c:sway_client_parse_workspace() "
            "failed to read member 'output'.");
    ws->output = g_strdup(json_node_get_string(node));

    // parse focused
    node = json_object_get_member(obj, "focused");
    if (!node)
        g_error(
            "sway_client.c:sway_client_parse_workspace() "
            "failed to read member 'focused'.");
    ws->focused = json_node_get_boolean(node);

    return 0;
}

static void free_workspace(gpointer data) {
    WMWorkspace *ws = (WMWorkspace *)data;
    g_free(ws->name);
    g_free(ws->output);
    g_free(ws);
}

GPtrArray *sway_client_ipc_get_workspaces_resp(sway_client_ipc_msg *msg) {
    GPtrArray *out = g_ptr_array_new_full(0, free_workspace);
    JsonParser *parser = NULL;
    JsonReader *reader = NULL;
    GError *error = NULL;
    guint workspaces_n = 0;

    g_debug("sway_client.c:sway_client_ipc_get_workspaces_resp() called");

    if (msg->size == 0) return NULL;

    parser = json_parser_new();
    if (!json_parser_load_from_data(parser, msg->payload, msg->size, &error)) {
        g_warning(
            "sway_client.c:sway_client_ipc_get_workspaces_resp() "
            "failed to parse json.");
        return NULL;
    }
    reader = json_reader_new(json_parser_get_root(parser));

    if (!json_reader_is_array(reader))
        g_error(
            "sway_client.c:sway_client_ipc_get_workspaces_resp() "
            "received non-array response.");

    workspaces_n = json_reader_count_elements(reader);
    g_debug(
        "sway_client.c:sway_client_ipc_get_workspaces_resp() "
        "received response with %d workspaces.",
        workspaces_n);

    for (int i = 0; i < workspaces_n; i++) {
        JsonNode *node;
        WMWorkspace *ws = g_malloc0(sizeof(WMWorkspace));

        if (!json_reader_read_element(reader, i))
            g_error(
                "sway_client.c:sway_client_ipc_get_workspaces_resp() "
                "failed to read element %d.",
                i);

        node = json_reader_get_current_node(reader);

        if (sway_client_parse_workspace_json(json_node_get_object(node), ws) !=
            0) {
            json_reader_end_element(reader);
            g_warning(
                "sway_client.c:sway_client_ipc_get_workspaces_resp() "
                "failed to parse workspace %d.",
                i);
            g_free(ws);
            continue;
        }

        // used only by workspace events to indicate a empty event.
        ws->empty = false;

        g_debug(
            "sway_client.c:sway_client_ipc_get_workspaces_resp() "
            "parsed workspace [%u] [%d] [%s] [urgent: %d] [focused: %d] "
            "[visible: "
            "%d] "
            "[output: %s].",
            ws->id, ws->num, ws->name, ws->urgent, ws->focused, ws->visible,
            ws->output);

        json_reader_end_element(reader);

        // add to output array
        g_ptr_array_add(out, ws);
    }

    g_object_unref(reader);
    g_object_unref(parser);
    g_free(msg->payload);

    return out;
}

int sway_client_ipc_subscribe_req(int socket_fd, int events[], guint len) {
    JsonBuilder *builder = json_builder_new();
    JsonGenerator *gen = json_generator_new();
    gsize size = 0;
    int ret = -1;

    g_debug("sway_client.c:sway_client_ipc_subscribe_req() called");

    if (len == 0) return -1;

    sway_client_ipc_msg msg = {.type = IPC_SUBSCRIBE};

    json_builder_begin_array(builder);

    for (int i = 0; i < len; i++) {
        switch (events[i]) {
            case IPC_EVENT_WORKSPACE:
                json_builder_add_string_value(builder, "workspace");
                break;
            case IPC_EVENT_MODE:
                json_builder_add_string_value(builder, "mode");
                break;
            case IPC_EVENT_WINDOW:
                json_builder_add_string_value(builder, "window");
                break;
            case IPC_EVENT_BARCONFIG_UPDATE:
                json_builder_add_string_value(builder, "barconfig_update");
                break;
            case IPC_EVENT_BINDING:
                json_builder_add_string_value(builder, "binding");
                break;
            case IPC_EVENT_SHUTDOWN:
                json_builder_add_string_value(builder, "shutdown");
                break;
            case IPC_EVENT_TICK:
                json_builder_add_string_value(builder, "tick");
                break;
            case IPC_EVENT_BAR_STATE_UPDATE:
                json_builder_add_string_value(builder, "bar_state_update");
                break;
            case IPC_EVENT_INPUT:
                json_builder_add_string_value(builder, "input");
                break;
        }
    }

    json_builder_end_array(builder);

    json_generator_set_root(gen, json_builder_get_root(builder));

    msg.payload = json_generator_to_data(gen, &size);
    if (size > 0xFFFFFFFF) {
        g_error(
            "sway_client.c:sway_client_ipc_subscribe_req() "
            "payload size is too large: %lu.",
            size);
    }
    msg.size = size;

    ret = sway_client_ipc_send(socket_fd, &msg);
    if (ret < 0) {
        g_warning(
            "sway_client.c:sway_client_ipc_subscribe_req() "
            "failed to send subscribe request.");
        g_free(msg.payload);
    }
    return ret;
}

gboolean sway_client_ipc_subscribe_resp(sway_client_ipc_msg *msg) {
    JsonParser *parser = NULL;
    JsonReader *reader = NULL;
    gboolean ok = false;

    if (msg->size == 0) return -1;

    parser = json_parser_new();
    if (!json_parser_load_from_data(parser, msg->payload, msg->size, NULL)) {
        g_warning(
            "sway_client.c:sway_client_ipc_subscribe_resp() "
            "failed to parse json.");
        return -1;
    }

    reader = json_reader_new(json_parser_get_root(parser));

    if (!json_reader_is_object(reader)) {
        g_warning(
            "sway_client.c:sway_client_ipc_subscribe_resp() "
            "received non-object response.");
        return -1;
    }

    json_reader_read_member(reader, "success");
    ok = json_reader_get_boolean_value(reader);

    g_object_unref(parser);
    g_object_unref(reader);
    g_free(msg->payload);
    return ok;
}

int sway_client_ipc_focus_workspace(int socket_fd, WMWorkspace *ws) {
    sway_client_ipc_msg msg = {.type = IPC_COMMAND};
    char *cmd = NULL;
    // sway will only convert numbers up to 2147483647 to "numbered" workspaces.
    // this requires 10 chars to represent as a string plus null byte.
    char itoa_buff[11];

    if (ws == NULL) {
        return -1;
    }

    if (ws->num == -1) {
        cmd = "workspace ";

        msg.size = strlen(cmd) + strlen(ws->name) + 1;
        msg.payload = g_malloc0(msg.size);

        strcpy(msg.payload, cmd);
        strcat(msg.payload, ws->name);
    } else {
        cmd = "workspace number ";

        snprintf(itoa_buff, sizeof(itoa_buff), "%d", ws->num);

        msg.size = strlen(cmd) + strlen(itoa_buff) + 1;
        msg.payload = g_malloc0(msg.size);

        strcpy(msg.payload, cmd);
        strcat(msg.payload, itoa_buff);
    }
    g_debug(
        "sway_client.c:sway_client_ipc_focus_workspace() sending command: %s",
        msg.payload);

    // send off message
    return sway_client_ipc_send(socket_fd, &msg);
}

WMWorkspaceEvent *sway_client_ipc_event_workspace_resp(
    sway_client_ipc_msg *msg) {
    JsonParser *parser = NULL;
    JsonReader *reader = NULL;
    JsonNode *ws_obj = NULL;
    WMWorkspaceEvent *ws = g_malloc0(sizeof(WMWorkspaceEvent));
    GError *error;
    const gchar *change;

    g_debug("sway_client.c:sway_client_ipc_event_workspace() called");

    if (msg->size == 0) {
        goto error;
    }

    parser = json_parser_new();
    if (!json_parser_load_from_data(parser, msg->payload, msg->size, &error)) {
        g_warning(
            "sway_client.c:sway_client_ipc_event_workspace() "
            "failed to parse json.");
        goto error;
    }

    reader = json_reader_new(json_parser_get_root(parser));
    if (!json_reader_is_object(reader)) {
        g_warning(
            "sway_client.c:sway_client_ipc_event_workspace() "
            "received non-object response. error: %s",
            json_reader_get_error(reader)->message);
        goto error;
    }

    // read 'changed' member
    if (!json_reader_read_member(reader, "change")) {
        g_warning(
            "sway_client.c:sway_client_ipc_event_workspace() "
            "received response without 'change' member. error: "
            "%s",
            json_reader_get_error(reader)->message);
        goto error;
    }
    change = json_reader_get_string_value(reader);

    // map to domain event
    ws->type = sway_client_event_map((gchar *)change);
    if (ws->type == -1) {
        g_warning(
            "sway_client.c:sway_client_ipc_event_workspace() "
            "received unknown event: %s",
            change);
        goto error;
    }

    g_debug(
        "sway_client.c:sway_client_ipc_event_workspace() workspace event: "
        "%s",
        change);

    // done reading 'changed' member, reset cursor.
    json_reader_end_member(reader);

    // short-circuit for reload event, there is no workspace details in this
    // event.
    if (g_strcmp0(change, "reload") == 0) {
        goto done;
    }

    // read 'current' member which holds workspace json node.
    if (!json_reader_read_member(reader, "current")) {
        g_warning(
            "sway_client.c:sway_client_ipc_event_workspace() "
            "received response without 'current' member. error: %s",
            json_reader_get_error(reader)->message);
        goto error;
    }

    // extract workspace json node
    ws_obj = json_reader_get_current_node(reader);
    if (!ws_obj) {
        g_error(
            "sway_client.c:sway_client_ipc_event_workspace() "
            "failed to get current node: %s",
            json_reader_get_error(reader)->message);
        goto error;
    }

    // feed it to our json parser.
    sway_client_parse_workspace_json(json_node_get_object(ws_obj),
                                     &ws->workspace);

    g_debug(
        "sway_client.c:sway_client_ipc_event_workspace() "
        "parsed workspace event [%s] for workspace ID [%d]",
        window_manager_service_event_to_string(ws->type), ws->workspace.id);

done:
    if (parser) g_object_unref(parser);
    if (reader) g_object_unref(reader);
    g_free(msg->payload);
    return ws;

error:
    if (parser) g_object_unref(parser);
    if (reader) g_object_unref(reader);
    g_free(msg->payload);
    g_free(ws);
    return NULL;
}
