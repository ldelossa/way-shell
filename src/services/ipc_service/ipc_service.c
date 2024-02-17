#include "ipc_service.h"

#include <adwaita.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "../../panel/message_tray/message_tray.h"
#include "../../services/wireplumber_service.h"
#include "glib-unix.h"
#include "ipc_commands.h"

#define IPC_SOCK "wlr-shell.sock"

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

    // TODO: really, we want to open the tray on the currently focused monitor
    // but GDK4 does not supply methods to find the "monitor at pointer"
    // anymore. come up with a better way to pick the focused monitor to open
    // this on, maybe by querying the window manager (sway, etc..)?
    GdkDisplay *display = gdk_display_get_default();
    GListModel *monitors = gdk_display_get_monitors(display);

    MessageTrayMediator *m = message_tray_get_global_mediator();
    message_tray_mediator_req_open(m, g_list_model_get_item(monitors, 0));
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

static gboolean on_ipc_readable(gint fd, GIOCondition condition,
                                gpointer user_data) {
    uint8_t buff[4096];
    gboolean ret = false;
    struct sockaddr_un saddr = {0};
    socklen_t size = sizeof(struct sockaddr_un);

    g_debug("ipc_service.c:on_ipc_readable() received IPC message");

    recvfrom(fd, buff, sizeof(buff), 0, (struct sockaddr *)&saddr, &size);

    IPCHeader *hdr = (IPCHeader *)&buff;

    switch (hdr->type) {
        case IPC_CMD_MESSAGE_TRAY_OPEN:
            g_debug(
                "ipc_service.c:on_ipc_readable() recieved "
                "IPC_CMD_MESSAGE_TRAY_OPEN");
            ret = ipc_cmd_message_tray_open();
            break;
        case IPC_CMD_VOLUME_UP:
            g_debug(
                "ipc_service.c:on_ipc_readable() recieved "
                "IPC_CMD_VOLUME_UP");
            ret = ipc_cmd_volume_up();
            break;
        case IPC_CMD_VOLUME_DOWN:
            g_debug(
                "ipc_service.c:on_ipc_readable() recieved "
                "IPC_CMD_VOLUME_DOWN");
            ret = ipc_cmd_volume_down();
            break;
        case IPC_CMD_VOLUME_SET:
            g_debug(
                "ipc_service.c:on_ipc_readable() recieved "
                "IPC_CMD_VOLUME_SET");
            ret = ipc_cmd_volume_set((IPCVolumeSet *)hdr);
            break;
        default:
            break;
    }
    return true;
}

static int ipc_service_setup_ipc_sock(IPCService *self) {
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
