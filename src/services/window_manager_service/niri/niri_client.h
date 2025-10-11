#pragma once

#include <adwaita.h>

#include "../window_manager_service.h"

gchar *niri_client_find_socket_path();
GIOChannel *niri_client_connect(gchar *socket_path);
int niri_client_send_request(GIOChannel *channel, const gchar *request_json, gchar **response_json);
GPtrArray *niri_client_get_workspaces(GIOChannel *channel);
GPtrArray *niri_client_get_outputs(GIOChannel *channel);
int niri_client_focus_workspace(GIOChannel *channel, const gchar *workspace_name);
int niri_client_move_window_to_workspace(GIOChannel *channel, const gchar *workspace_name);
int niri_client_rename_current_workspace(GIOChannel *channel, const gchar *new_name);
int niri_client_move_workspace_to_output(GIOChannel *channel, const gchar *output_name);
int niri_client_subscribe_events(GIOChannel *channel);
WMWorkspaceEventType niri_client_event_map(const gchar *event_type);
gint compare_workspaces_by_idx(gconstpointer a, gconstpointer b);
