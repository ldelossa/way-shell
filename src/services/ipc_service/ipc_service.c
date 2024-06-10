#include "ipc_service.h"

#include <adwaita.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "../../../gresources.h"
#include "../../activities/activities.h"
#include "../../app_switcher/app_switcher.h"
#include "../../output_switcher/output_switcher.h"
#include "../../panel/message_tray/message_tray.h"
#include "../../services/brightness_service/brightness_service.h"
#include "../../services/theme_service.h"
#include "../../services/wireplumber_service.h"
#include "../../workspace_switcher/workspace_switcher.h"
#include "glib-unix.h"
#include "ipc_commands.h"

#define IPC_SOCK "way-shell.sock"

static IPCService *global = NULL;

enum signals { signals_n };

struct _IPCService {
    GObject parent_instance;
    int socket;
};
static guint signals[signals_n] = {0};
G_DEFINE_TYPE(IPCService, ipc_service, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init methods
static void ipc_service_dispose(GObject *gobject) {
    IPCService *self = IPC_SERVICE(gobject);

    // Chain-up
    G_OBJECT_CLASS(ipc_service_parent_class)->dispose(gobject);
};

static void ipc_service_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(ipc_service_parent_class)->finalize(gobject);
};

static void ipc_service_class_init(IPCServiceClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = ipc_service_dispose;
    object_class->finalize = ipc_service_finalize;
};

static gboolean ipc_cmd_message_tray_open() {
    g_debug("ipc_service.c:ipc_cmd_message_tray_open()");

    MessageTray *m = message_tray_get_global();
    message_tray_set_visible(m);
    return true;
}

static gboolean ipc_cmd_volume_up() {
    g_debug("ipc_service.c:ipc_cmd_volume_up()");

    WirePlumberService *wp = wire_plumber_service_get_global();
    if (!wp) {
        g_critical(
            "ipc_service.c:ipc_cmd_volume_up() failed to get wireplumber "
            "service");
        return false;
    }
    WirePlumberServiceNode *sink = wire_plumber_service_get_default_sink(wp);
    wire_plumber_service_volume_up(wp, sink);
    return true;
}

static gboolean ipc_cmd_volume_down() {
    g_debug("ipc_service.c:ipc_cmd_volume_down()");
    WirePlumberService *wp = wire_plumber_service_get_global();
    if (!wp) {
        g_critical(
            "ipc_service.c:ipc_cmd_volume_up() failed to get wireplumber "
            "service");
        return false;
    }
    WirePlumberServiceNode *sink = wire_plumber_service_get_default_sink(wp);
    wire_plumber_service_volume_down(wp, sink);
    return true;
}

static gboolean ipc_cmd_volume_set(IPCVolumeSet *msg) {
    g_debug("ipc_service.c:ipc_cmd_volume_set()");
    WirePlumberService *wp = wire_plumber_service_get_global();
    if (!wp) {
        g_critical(
            "ipc_service.c:ipc_cmd_volume_up() failed to get wireplumber "
            "service");
        return true;
    }
    WirePlumberServiceNode *sink = wire_plumber_service_get_default_sink(wp);
    wire_plumber_service_set_volume(wp, sink, msg->volume);
    return false;
}

static gboolean ipc_cmd_volume_mute(IPCVolumeMute *msg) {
    g_debug("ipc_service.c:ipc_cmd_volume_mute()");
    WirePlumberService *wp = wire_plumber_service_get_global();
    if (!wp) {
        g_critical(
            "ipc_service.c:ipc_cmd_volume_up() failed to get wireplumber "
            "service");
        return false;
    }
    WirePlumberServiceNode *sink = wire_plumber_service_get_default_sink(wp);
    if (sink->mute)
        wire_plumber_service_volume_unmute(wp, sink);
    else
        wire_plumber_service_volume_mute(wp, sink);
    return true;
}

static gboolean ipc_cmd_brightness_up() {
    g_debug("ipc_service.c:ipc_cmd_brightness_up()");
    BrightnessService *b = brightness_service_get_global();
    if (!b) {
        g_critical(
            "ipc_service.c:ipc_cmd_brightness_up() failed to get brightness "
            "service");
        return false;
    }
    brightness_service_up(b);
    return true;
}

static gboolean ipc_cmd_brightness_down() {
    g_debug("ipc_service.c:ipc_cmd_brightness_down()");
    BrightnessService *b = brightness_service_get_global();
    if (!b) {
        g_critical(
            "ipc_service.c:ipc_cmd_brightness_down() failed to get brightness "
            "service");
        return false;
    }
    brightness_service_down(b);
    return true;
}

static gboolean ipc_cmd_theme_dark() {
    g_debug("ipc_service.c:ipc_cmd_theme_dark()");
    ThemeService *t = theme_service_get_global();
    if (!t) {
        g_critical(
            "ipc_service.c:ipc_cmd_theme_dark() failed to get theme service");
        return false;
    }
    theme_service_set_dark_theme(t, true);
    return true;
}

static gboolean ipc_cmd_theme_light() {
    g_debug("ipc_service.c:ipc_cmd_theme_light()");
    ThemeService *t = theme_service_get_global();
    if (!t) {
        g_critical(
            "ipc_service.c:ipc_cmd_theme_light() failed to get theme service");
        return false;
    }
    theme_service_set_light_theme(t, true);
    return true;
}

static gboolean ipc_cmd_dump_dark_theme() {
    // make config directory if it does not exist
    gchar *config_dir =
        g_build_filename(g_get_user_config_dir(), "way-shell", NULL);
    g_mkdir_with_parents(config_dir, 0700);

    // dump dark theme gresource back to file
    gchar *dark_theme_path =
        g_build_filename(config_dir, "way-shell-dark.css", NULL);
    GResource *res = gresources_get_resource();
    GBytes *dark_theme_bytes = g_resource_lookup_data(
        res, "/org/ldelossa/way-shell/data/theme/way-shell-dark.css", 0, NULL);
    gsize size;
    const gchar *dark_theme = g_bytes_get_data(dark_theme_bytes, &size);

    GError *error = NULL;
    g_file_set_contents(dark_theme_path, dark_theme, size, &error);
    if (error) {
        g_debug(
            "ipc_service.c:ipc_cmd_dump_dark_theme() failed to dump dark theme "
            "%s",
            error->message);
        return false;
    }
    return true;
};

static gboolean ipc_cmd_dump_light_theme() {
    // make config directory if it does not exist
    gchar *config_dir =
        g_build_filename(g_get_user_config_dir(), "way-shell", NULL);
    g_mkdir_with_parents(config_dir, 0700);

    // dump light theme gresource back to file
    gchar *light_theme_path =
        g_build_filename(config_dir, "way-shell-light.css", NULL);

    GResource *res = gresources_get_resource();
    GBytes *light_theme_bytes = g_resource_lookup_data(
        res, "/org/ldelossa/way-shell/data/theme/way-shell-light.css", 0, NULL);
    gsize size;
    const gchar *light_theme = g_bytes_get_data(light_theme_bytes, &size);

    GError *error = NULL;
    g_file_set_contents(light_theme_path, light_theme, size, &error);
    if (error) {
        g_debug(
            "ipc_service.c:ipc_cmd_dump_light_theme() failed to dump light "
            "theme %s",
            error->message);
        return false;
    }
    return true;
};

static gboolean ip_cmd_activities_show() {
    g_debug("ipc_service.c:ip_cmd_activities_show()");

    Activities *a = activities_get_global();
    if (!a) {
        return false;
    }

    activities_show(a);

    return true;
}

static gboolean ip_cmd_activities_hide() {
    g_debug("ipc_service.c:ip_cmd_activities_hide()");

    Activities *a = activities_get_global();
    if (!a) {
        return false;
    }

    activities_hide(a);

    return true;
}

static gboolean ip_cmd_activities_toggle() {
    g_debug("ipc_service.c:ip_cmd_activities_toggle()");

    Activities *a = activities_get_global();
    if (!a) {
        return false;
    }

    activities_toggle(a);

    return true;
}

static gboolean ip_cmd_app_switcher_show() {
    g_debug("ipc_service.c:ip_cmd_app_switcher_show()");

    AppSwitcher *a = app_switcher_get_global();
    if (!a) {
        return false;
    }

    app_switcher_show(a);

    return true;
}

static gboolean ip_cmd_app_switcher_hide() {
    g_debug("ipc_service.c:ip_cmd_app_switcher_hide()");

    AppSwitcher *a = app_switcher_get_global();
    if (!a) {
        return false;
    }

    app_switcher_hide(a);

    return true;
}

static gboolean ip_cmd_app_switcher_toggle() {
    g_debug("ipc_service.c:ip_cmd_app_switcher_toggle()");

    AppSwitcher *a = app_switcher_get_global();
    if (!a) {
        return false;
    }

    app_switcher_toggle(a);

    return true;
}

static gboolean ip_cmd_output_switcher_show() {
    g_debug("ipc_service.c:ip_cmd_output_switcher_show()");

    OutputSwitcher *o = output_switcher_get_global();
    if (!o) {
        return false;
    }

    output_switcher_show(o);

    return true;
}

static gboolean ip_cmd_output_switcher_hide() {
    g_debug("ipc_service.c:ip_cmd_output_switcher_hide()");

    OutputSwitcher *o = output_switcher_get_global();
    if (!o) {
        return false;
    }

    output_switcher_hide(o);

    return true;
}

static gboolean ip_cmd_output_switcher_toggle() {
    g_debug("ipc_service.c:ip_cmd_output_switcher_toggle()");

    OutputSwitcher *o = output_switcher_get_global();
    if (!o) {
        return false;
    }

    output_switcher_toggle(o);

    return true;
}

static gboolean ip_cmd_workspace_switcher_show() {
    g_debug("ipc_service.c:ip_cmd_workspace_switcher_show()");

    WorkspaceSwitcher *a = workspace_switcher_get_global();
    if (!a) {
        return false;
    }

    workspace_switcher_show(a);

    return true;
}

static gboolean ip_cmd_workspace_switcher_hide() {
    g_debug("ipc_service.c:ip_cmd_workspace_switcher_hide()");

    WorkspaceSwitcher *a = workspace_switcher_get_global();
    if (!a) {
        return false;
    }

    workspace_switcher_hide(a);

    return true;
}

static gboolean ip_cmd_workspace_switcher_toggle() {
    g_debug("ipc_service.c:ip_cmd_workspace_switcher_toggle()");

    WorkspaceSwitcher *a = workspace_switcher_get_global();
    if (!a) {
        return false;
    }

    workspace_switcher_toggle(a);

    return true;
}

static gboolean ip_cmd_workspace_app_switcher_show() {
    g_debug("ipc_service.c:ip_cmd_workspace_switcher_show()");

    WorkspaceSwitcher *a = workspace_switcher_get_global();
    if (!a) {
        return false;
    }

    workspace_switcher_show_app_mode(a);

    return true;
}

static gboolean ip_cmd_workspace_app_switcher_hide() {
    g_debug("ipc_service.c:ip_cmd_workspace_switcher_hide()");

    WorkspaceSwitcher *a = workspace_switcher_get_global();
    if (!a) {
        return false;
    }

    workspace_switcher_hide(a);

    return true;
}

static gboolean ip_cmd_workspace_app_switcher_toggle() {
    g_debug("ipc_service.c:ip_cmd_workspace_switcher_toggle()");

    WorkspaceSwitcher *a = workspace_switcher_get_global();
    if (!a) {
        return false;
    }

    workspace_switcher_toggle_app_mode(a);

    return true;
}

static gboolean ipc_command_bluelight_filter_enable() {
    g_debug("ipc_service.c:ipc_command_bluelight_filter_enable()");

    WaylandService *w = wayland_service_get_global();
    if (!w) {
        return false;
    }

    wayland_wlr_bluelight_filter(w, 3100);

    return true;
}

static gboolean ipc_command_bluelight_filter_disable() {
    g_debug("ipc_service.c:ipc_command_bluelight_filter_disable()");

    WaylandService *w = wayland_service_get_global();
    if (!w) {
        return false;
    }

    wayland_wlr_bluelight_filter_destroy(w);

    return true;
}

static gboolean on_ipc_readable(gint fd, GIOCondition condition,
                                gpointer user_data) {
    uint8_t buff[4096];
    gboolean ret = false;
    struct sockaddr_un saddr = {0};
    socklen_t size = sizeof(struct sockaddr_un);

    g_debug("ipc_service.c:on_ipc_readable() received IPC message");

    if (recvfrom(fd, buff, sizeof(buff), 0, (struct sockaddr *)&saddr, &size) ==
        -1) {
        g_critical("ipc_service.c:on_ipc_readable() failed to recvfrom()");
        return true;
    }

    // client is an abstract unix socket, debug the client socket's path
    g_debug("ipc_service.c:on_ipc_readable() received IPC message from %s",
            &saddr.sun_path[1]);
    ret = false;

    IPCHeader *hdr = (IPCHeader *)&buff;

    switch (hdr->type) {
        case IPC_CMD_MESSAGE_TRAY_OPEN:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_MESSAGE_TRAY_OPEN");
            ret = ipc_cmd_message_tray_open();
            break;
        case IPC_CMD_VOLUME_UP:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_VOLUME_UP");
            ret = ipc_cmd_volume_up();
            break;
        case IPC_CMD_VOLUME_DOWN:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_VOLUME_DOWN");
            ret = ipc_cmd_volume_down();
            break;
        case IPC_CMD_VOLUME_SET:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_VOLUME_SET");
            ret = ipc_cmd_volume_set((IPCVolumeSet *)hdr);
            break;
        case IPC_CMD_VOLUME_MUTE:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_VOLUME_MUTE");
            ret = ipc_cmd_volume_mute((IPCVolumeMute *)hdr);
            break;
        case IPC_CMD_BRIGHTNESS_UP:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_BRIGHTNESS_UP");
            ret = ipc_cmd_brightness_up();
            break;
        case IPC_CMD_BRIGHTNESS_DOWN:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_BRIGHTNESS_DOWN");
            ret = ipc_cmd_brightness_down();
            break;
        case IPC_CMD_THEME_DARK:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_THEME_DARK");
            ret = ipc_cmd_theme_dark();
            break;
        case IPC_CMD_THEME_LIGHT:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_THEME_LIGHT");
            ret = ipc_cmd_theme_light();
            break;
        case IPC_CMD_DUMP_DARK_THEME:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_DUMP_DARK_THEME");
            ret = ipc_cmd_dump_dark_theme();
            break;
        case IPC_CMD_DUMP_LIGHT_THEME:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_DUMP_LIGHT_THEME");
            ret = ipc_cmd_dump_light_theme();
            break;
        case IPC_CMD_ACTIVITIES_SHOW:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_ACTIVITIES_SHOW");
            ret = ip_cmd_activities_show();
            break;
        case IPC_CMD_ACTIVITIES_HIDE:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_ACTIVITIES_HIDE");
            ret = ip_cmd_activities_hide();
            break;
        case IPC_CMD_ACTIVITIES_TOGGLE:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_ACTIVITIES_TOGGLE");
            ret = ip_cmd_activities_toggle();
            break;
        case IPC_CMD_APP_SWITCHER_SHOW:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_APP_SWITCHER_SHOW");
            ret = ip_cmd_app_switcher_show();
            break;
        case IPC_CMD_APP_SWITCHER_HIDE:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_APP_SWITCHER_HIDE");
            ret = ip_cmd_app_switcher_hide();
            break;
        case IPC_CMD_APP_SWITCHER_TOGGLE:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_APP_SWITCHER_TOGGLE");
            ret = ip_cmd_app_switcher_toggle();
            break;
        case IPC_CMD_WORKSPACE_SWITCHER_SHOW:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_WORKSPACE_SWITCHER_SHOW");
            ret = ip_cmd_workspace_switcher_show();
            break;
        case IPC_CMD_WORKSPACE_SWITCHER_HIDE:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_WORKSPACE_SWITCHER_HIDE");
            ret = ip_cmd_workspace_switcher_hide();
            break;
        case IPC_CMD_WORKSPACE_SWITCHER_TOGGLE:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_WORKSPACE_SWITCHER_TOGGLE");
            ret = ip_cmd_workspace_switcher_toggle();
            break;
        case IPC_CMD_OUTPUT_SWITCHER_SHOW:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_OUTPUT_SWITCHER_SHOW");
            ret = ip_cmd_output_switcher_show();
            break;
        case IPC_CMD_OUTPUT_SWITCHER_HIDE:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_OUTPUT_SWITCHER_HIDE");
            ret = ip_cmd_output_switcher_hide();
            break;
        case IPC_CMD_OUTPUT_SWITCHER_TOGGLE:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_OUTPUT_SWITCHER_TOGGLE");
            ret = ip_cmd_output_switcher_toggle();
            break;
        case IPC_CMD_WORKSPACE_APP_SWITCHER_SHOW:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_WORKSPACE_APP_SWITCHER_SHOW");
            ret = ip_cmd_workspace_app_switcher_show();
            break;
        case IPC_CMD_WORKSPACE_APP_SWITCHER_HIDE:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_WORKSPACE_APP_SWITCHER_HIDE");
            ret = ip_cmd_workspace_app_switcher_hide();
            break;
        case IPC_CMD_WORKSPACE_APP_SWITCHER_TOGGLE:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_WORKSPACE_APP_SWITCHER_TOGGLE");
            ret = ip_cmd_workspace_app_switcher_toggle();
            break;
        case IPC_CMD_BLUELIGHT_FILTER_ENABLE:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_BLUELIGHT_FILTER_ENABLE");
            ret = ipc_command_bluelight_filter_enable();
            break;
        case IPC_CMD_BLUELIGHT_FILTER_DISABLE:
            g_debug(
                "ipc_service.c:on_ipc_readable() received "
                "IPC_CMD_BLUELIGHT_FILTER_DISABLE");
            ret = ipc_command_bluelight_filter_disable();
            break;
        default:
            goto skip_resp;
            break;
    }

    // set ret as a response back to client, its a simple one byte boolean.
    sendto(fd, &ret, sizeof(ret), 0, (struct sockaddr *)&saddr, size);

skip_resp:
    return true;
}

static int ipc_service_setup_ipc_sock(IPCService *self) {
    g_debug("ipc_service.c:ipc_service_setup_ipc_sock() called");

    // we need to find an "XDG_RUNTIME_DIR" for the current user
    char *xdg_runtime_dir = g_strdup(g_getenv("XDG_RUNTIME_DIR"));
    if (!xdg_runtime_dir) {
        g_critical(
            "ipc_service.c:ipc_service_setup_ipc_sock() XDG_RUNTIME_DIR is not "
            "set");
        return -1;
    }

    // delete stale socket if it exists.
    GFile *socket_path =
        g_file_new_build_filename(xdg_runtime_dir, IPC_SOCK, NULL);
    g_file_delete(socket_path, NULL, NULL);

    // setup a unix socket at our xdg runtime path
    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
        .sun_path = {0},
    };
    g_strlcpy((gchar *)&(addr.sun_path), g_file_get_path(socket_path),
              sizeof(addr.sun_path));

    int sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock <= 0) {
        g_critical(
            "ipc_service.c:ipc_service_setup_ipc_sock() failed to create unix "
            "socket");
        return -1;
    }

    if (bind(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) != 0) {
        g_critical(
            "ipc_service.c:ipc_service_setup_ipc_sock() failed to create unix "
            "socket");
        return -1;
    }

    self->socket = sock;

    // add as a g source
    g_unix_fd_add(self->socket, G_IO_IN, on_ipc_readable, self);

    return self->socket;
}

static void ipc_service_init(IPCService *self) {
    self->socket = -1;
    ipc_service_setup_ipc_sock(self);
}

int ipc_service_global_init() {
    global = g_object_new(IPC_SERVICE_TYPE, NULL);
    if (global->socket == -1) global = NULL;
    g_object_unref(global);
    return 0;
}

IPCService *ipc_service_get_global() { return global; }
