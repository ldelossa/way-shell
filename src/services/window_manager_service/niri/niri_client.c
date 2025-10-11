#include "niri_client.h"

#include <adwaita.h>
#include <json-glib/json-glib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../../window_manager_service/window_manager_service.h"

WMWorkspaceEventType niri_client_event_map(const gchar *event_type) {
    if (g_strcmp0(event_type, "WorkspaceAdded") == 0) return WMWORKSPACE_EVENT_CREATED;
    if (g_strcmp0(event_type, "WorkspaceRemoved") == 0) return WMWORKSPACE_EVENT_DESTROYED;
    if (g_strcmp0(event_type, "WorkspaceActivated") == 0) return WMWORKSPACE_EVENT_FOCUSED;
    if (g_strcmp0(event_type, "WorkspaceMoved") == 0) return WMWORKSPACE_EVENT_MOVED;
    if (g_strcmp0(event_type, "WorkspaceRenamed") == 0) return WMWORKSPACE_EVENT_RENAMED;
    return -1;
}

gchar *niri_client_find_socket_path() {
    char *socket_path;

    g_debug("niri_client.c:niri_client_find_socket_path() called");

    socket_path = getenv("NIRI_SOCKET");
    if (!socket_path) {
        g_warning("NIRI_SOCKET environment variable not set");
        return nullptr;
    }

    return g_strdup(socket_path);
}

GIOChannel *niri_client_connect(gchar *socket_path) {
    int socket_fd = -1;
    g_debug("niri_client.c:niri_client_connect() called");

    if (!socket_path) return nullptr;

    struct sockaddr_un addr = {0};

    if ((strlen(socket_path) + 1) > sizeof(addr.sun_path))
        return nullptr;

    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, socket_path);

    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd == -1) return nullptr;

    if (connect(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        return nullptr;

    GIOChannel *channel = g_io_channel_unix_new(socket_fd);
    GError *error = nullptr;
    g_io_channel_set_encoding(channel, "UTF-8", &error);
    if (error) {
        g_error("niri_client.c:niri_client_connect() failed to set encoding: %s", error->message);
        g_error_free(error);
        return nullptr;
    }
    return channel;
}

int niri_client_send_request(GIOChannel *channel, const gchar *request_json, gchar **response_json) {
    g_debug("niri_client.c:niri_client_send_request() sending: %s", request_json);

    // Send request with newline
    gchar *request_with_newline = g_strdup_printf("%s\n", request_json);
    gsize bytes_written;
    GError *error = nullptr;
    GIOStatus status = g_io_channel_write_chars(channel, request_with_newline, -1, &bytes_written, &error);
    if (status == G_IO_STATUS_ERROR) {
        g_warning("niri_client.c:niri_client_send_request() send failed: %s", error->message);
        g_error_free(error);
        return -1;
    }
    error = nullptr;
    g_io_channel_flush(channel, &error);
    if (status == G_IO_STATUS_ERROR) {
        g_warning("niri_client.c:niri_client_send_request() flush failed: %s", error->message);
        g_error_free(error);
        return -1;
    }
    g_free(request_with_newline);

    // Read response line
    gsize bytes_read;
    gsize terminator_pos;
    status = g_io_channel_read_line(channel, response_json, &bytes_read, &terminator_pos, &error);

    if (status == G_IO_STATUS_ERROR) {
        g_warning("niri_client.c:niri_client_send_request() recv failed: %s", error->message);
        g_error_free(error);
        return -1;
    }

    g_debug("niri_client.c:niri_client_send_request() received: %s", *response_json);
    return 0;
}

GPtrArray *niri_client_get_workspaces(GIOChannel *channel) {
    g_debug("niri_client.c:niri_client_get_workspaces() called");

    gchar *response_json = nullptr;
    if (niri_client_send_request(channel, "\"Workspaces\"", &response_json) != 0) {
        return nullptr;
    }

    JsonParser *parser = json_parser_new();
    GError *error = nullptr;

    if (!json_parser_load_from_data(parser, response_json, -1, &error)) {
        g_warning("niri_client.c:niri_client_get_workspaces() JSON parse error: %s", error->message);
        g_error_free(error);
        g_object_unref(parser);
        g_free(response_json);
        return nullptr;
    }

    JsonNode *root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_OBJECT(root)) {
        g_warning("niri_client.c:niri_client_get_workspaces() root is not an object");
        g_object_unref(parser);
        g_free(response_json);
        return nullptr;
    }
    JsonObject *root_obj = json_node_get_object(root);

    JsonNode *ok_node = json_object_get_member(root_obj, "Ok");
    if (!ok_node || !JSON_NODE_HOLDS_OBJECT(ok_node)) {
        g_warning("niri_client.c:niri_client_get_workspaces(): no Ok object found");
        g_object_unref(parser);
        g_free(response_json);
        return nullptr;
    }
    JsonObject *ok_obj = json_node_get_object(ok_node);

    JsonNode *workspaces_node = json_object_get_member(ok_obj, "Workspaces");
    if (!workspaces_node || !JSON_NODE_HOLDS_ARRAY(workspaces_node)) {
        g_warning("niri_client.c:niri_client_get_workspaces() no Workspaces array found");
        g_object_unref(parser);
        g_free(response_json);
        return nullptr;
    }
    JsonArray *workspaces_array = json_node_get_array(workspaces_node);

    GPtrArray *workspaces = g_ptr_array_new_with_free_func(g_free);
    for (guint i = 0; i < json_array_get_length(workspaces_array); i++) {
        JsonNode *ws_node = json_array_get_element(workspaces_array, i);
        if (!JSON_NODE_HOLDS_OBJECT(ws_node)) continue;

        JsonObject *ws_obj = json_node_get_object(ws_node);
        WMWorkspace *ws = g_new0(WMWorkspace, 1);

        JsonNode *name_node = json_object_get_member(ws_obj, "name");
        if (name_node && JSON_NODE_HOLDS_VALUE(name_node)) {
            ws->name = g_strdup(json_node_get_string(name_node));
        } else {
            // Use idx as fallback name
            JsonNode *idx_node = json_object_get_member(ws_obj, "idx");
            if (idx_node && JSON_NODE_HOLDS_VALUE(idx_node)) {
                ws->name = g_strdup_printf("%" G_GINT64_FORMAT, json_node_get_int(idx_node));
                ws->num = json_node_get_int(idx_node);
            }
        }

        JsonNode *id_node = json_object_get_member(ws_obj, "id");
        if (id_node && JSON_NODE_HOLDS_VALUE(id_node)) {
            ws->id = json_node_get_int(id_node);
        }

        JsonNode *output_node = json_object_get_member(ws_obj, "output");
        if (output_node && JSON_NODE_HOLDS_VALUE(output_node)) {
            ws->output = g_strdup(json_node_get_string(output_node));
        }

        JsonNode *is_active_node = json_object_get_member(ws_obj, "is_active");
        if (is_active_node && JSON_NODE_HOLDS_VALUE(is_active_node)) {
            ws->focused = json_node_get_boolean(is_active_node);
        }

        JsonNode *is_focused_node = json_object_get_member(ws_obj, "is_focused");
        if (is_focused_node && JSON_NODE_HOLDS_VALUE(is_focused_node)) {
            ws->visible = json_node_get_boolean(is_focused_node);
        }

        JsonNode *is_urgent_node = json_object_get_member(ws_obj, "is_urgent");
        if (is_urgent_node && JSON_NODE_HOLDS_VALUE(is_urgent_node)) {
            ws->urgent = json_node_get_boolean(is_urgent_node);
        }

        g_ptr_array_add(workspaces, ws);
    }

    // Sort workspaces by idx field
    g_ptr_array_sort(workspaces, (GCompareFunc)compare_workspaces_by_idx);

    g_object_unref(parser);
    g_free(response_json);
    return workspaces;
}

gint compare_workspaces_by_idx(gconstpointer a, gconstpointer b) {
    const WMWorkspace *ws_a = *(const WMWorkspace **)a;
    const WMWorkspace *ws_b = *(const WMWorkspace **)b;

    return ws_a->num - ws_b->num;
}

GPtrArray *niri_client_get_outputs(GIOChannel *channel) {
    g_debug("niri_client.c:niri_client_get_outputs() called");

    gchar *response_json = nullptr;
    if (niri_client_send_request(channel, "\"Outputs\"", &response_json) != 0) {
        return nullptr;
    }

    JsonParser *parser = json_parser_new();
    GError *error = nullptr;

    if (!json_parser_load_from_data(parser, response_json, -1, &error)) {
        g_warning("niri_client.c:niri_client_get_outputs() JSON parse error: %s", error->message);
        g_error_free(error);
        g_object_unref(parser);
        g_free(response_json);
        return nullptr;
    }

    JsonNode *root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_OBJECT(root)) {
        g_warning("niri_client.c:niri_client_get_outputs() root is not an object");
        g_object_unref(parser);
        g_free(response_json);
        return nullptr;
    }
    JsonObject *root_obj = json_node_get_object(root);

    // Iterate over output names (keys in the object)
    GPtrArray *outputs = g_ptr_array_new_with_free_func(g_free);
    GList *output_names = json_object_get_members(root_obj);
    for (GList *iter = output_names; iter != nullptr; iter = iter->next) {
        const gchar *output_name = (const gchar *)iter->data;
        JsonNode *output_node = json_object_get_member(root_obj, output_name);

        if (!JSON_NODE_HOLDS_OBJECT(output_node)) continue;

        JsonObject *output_obj = json_node_get_object(output_node);
        WMOutput *output = g_new0(WMOutput, 1);

        // Name is the key
        output->name = g_strdup(output_name);

        JsonNode *make_node = json_object_get_member(output_obj, "make");
        if (make_node && JSON_NODE_HOLDS_VALUE(make_node)) {
            output->make = g_strdup(json_node_get_string(make_node));
        }

        JsonNode *model_node = json_object_get_member(output_obj, "model");
        if (model_node && JSON_NODE_HOLDS_VALUE(model_node)) {
            output->model = g_strdup(json_node_get_string(model_node));
        }

        // TODO: output->serial is wrapped in an Option, handle this later
        // TODO: implement output->current_workspace later

        g_ptr_array_add(outputs, output);
    }
    g_list_free(output_names);

    g_object_unref(parser);
    g_free(response_json);
    return outputs;
}

int niri_client_focus_workspace(GIOChannel *channel, const gchar *workspace_name) {
    g_debug("niri_client.c:niri_client_focus_workspace() workspace: %s", workspace_name);

    gchar *request;
    // For niri, we use WorkspaceReferenceArg which can be index or name
    // If workspace_name is numeric, use it as index, otherwise use as name
    if (g_ascii_isdigit(workspace_name[0])) {
        // Use as index
        request = g_strdup_printf("{\"Action\":{\"FocusWorkspace\":{\"reference\":{\"Index\":%s}}}}", workspace_name);
    } else {
        // Use as name
        request = g_strdup_printf("{\"Action\":{\"FocusWorkspace\":{\"reference\":{\"Name\":\"%s\"}}}}", workspace_name);
    }
    gchar *response = nullptr;

    int result = niri_client_send_request(channel, request, &response);

    g_free(request);
    g_free(response);
    return result;
}

int niri_client_move_window_to_workspace(GIOChannel *channel, const gchar *workspace_name) {
    g_debug("niri_client.c:niri_client_move_window_to_workspace() workspace: %s", workspace_name);

    gchar *request;
    // For niri, we use WorkspaceReferenceArg which can be index or name
    // If workspace_name is numeric, use it as index, otherwise use as name
    if (g_ascii_isdigit(workspace_name[0])) {
        // Use as index
        request = g_strdup_printf("{\"Action\":{\"MoveWindowToWorkspace\":{\"window_id\":null,\"reference\":{\"Index\":%s},\"focus\":false}}}", workspace_name);
    } else {
        // Use as name
        request = g_strdup_printf("{\"Action\":{\"MoveWindowToWorkspace\":{\"window_id\":null,\"reference\":{\"Name\":\"%s\"},\"focus\":false}}}", workspace_name);
    }
    gchar *response = nullptr;

    int result = niri_client_send_request(channel, request, &response);

    g_free(request);
    g_free(response);
    return result;
}

int niri_client_rename_current_workspace(GIOChannel *channel, const gchar *new_name) {
    g_debug("niri_client.c:niri_client_rename_workspace() new_name: %s", new_name);

    gchar *request = g_strdup_printf("{\"Action\":{\"SetWorkspaceName\":{\"name\":\"%s\",\"workspace\":null}}}", new_name);
    gchar *response = nullptr;

    int result = niri_client_send_request(channel, request, &response);

    g_free(request);
    g_free(response);
    return result;
}

int niri_client_move_workspace_to_output(GIOChannel *channel, const gchar *output_name) {
    g_debug("niri_client.c:niri_client_move_workspace_to_output() output: %s", output_name);

    gchar *request = g_strdup_printf("{\"Action\":{\"MoveWorkspaceToMonitor\":{\"output\":\"%s\",\"reference\":null}}}", output_name);
    gchar *response = nullptr;

    int result = niri_client_send_request(channel, request, &response);

    g_free(request);
    g_free(response);
    return result;
}

int niri_client_subscribe_events(GIOChannel *channel) {
    g_debug("niri_client.c:niri_client_subscribe_events() called");

    gchar *response = nullptr;
    return niri_client_send_request(channel, "\"EventStream\"", &response);
}
