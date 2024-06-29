#include "logind_service.h"

#include <adwaita.h>

#include "../../services/dbus_service.h"
#include "logind_manager_dbus.h"
#include "logind_session_dbus.h"

static LogindService *global = NULL;

enum signals { idle_inhibitor_changed, signals_n };

struct _LogindService {
    GObject parent_instance;
    DbusLogin1Manager *manager;
    DbusLogin1Session *session;
    GDBusConnection *conn;
    const char *active_profile;
    GArray *profiles;
    gboolean enabled;
    int idle_inhibitor_fd;
    // org.ldelossa.way-shell.system : interested settings:
    // idle-inhibitor
    GSettings *settings;
};
static guint signals[signals_n] = {0};
G_DEFINE_TYPE(LogindService, logind_service, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init methods
static void logind_service_dispose(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(logind_service_parent_class)->dispose(gobject);
};

static void logind_service_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(logind_service_parent_class)->finalize(gobject);
};

static void logind_service_class_init(LogindServiceClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = logind_service_dispose;
    object_class->finalize = logind_service_finalize;

    signals[idle_inhibitor_changed] = g_signal_new(
        "idle-inhibitor-changed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
        0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
};

static void logind_service_session_dbus_connect(LogindService *self) {
    g_debug("logind_service.c:logind_service_session_dbus_connect():");

    int uid = getuid();
    GError *error = NULL;
    GVariant *sessions = NULL;
    char *session_obj_path = NULL;
    char *session_id = 0;

    dbus_login1_manager_call_list_sessions_sync(self->manager, &sessions, NULL,
                                                &error);
    if (error) {
        g_error(
            "logind_service.c:logind_service_init(): error: list sessions: %s",
            error->message);
    }

    DbusLogin1Session *session = NULL;
    GVariant *session_variant = NULL;
    GVariantIter *iter;
    g_variant_get(sessions, "a(susso)", &iter);
    while (g_variant_iter_loop(iter, "@(susso)", &session_variant)) {
        gchar *id;
        guint32 sess_uid;
        gchar *seat;
        gchar *display;
        gchar *obj_path;
        g_variant_get(session_variant, "(susso)", &id, &sess_uid, &seat,
                      &display, &obj_path);

        if (!seat) continue;

        session = dbus_login1_session_proxy_new_sync(
            self->conn, G_DBUS_PROXY_FLAGS_NONE, "org.freedesktop.login1",
            obj_path, NULL, &error);

        if (error) {
            g_error(
                "logind_service.c:logind_service_init(): failed to create "
                "session "
                "proxy: %s",
                error->message);
        }

        char *state = NULL;
        g_object_get(session, "state", &state, NULL);

        gboolean active = (g_strcmp0(state, "active") == 0);
        g_free(state);

        if (active) {
            self->session = session;
            break;
        } else
            g_object_unref(session);
    }
    g_variant_unref(sessions);
    g_variant_iter_free(iter);

    if (!self->session)
        g_error(
            "logind_service.c:logind_service_init(): error: no session found");

    g_debug(
        "logind_service.c:logind_service_init(): found session id: %s "
        "session_obj_path: %s",
        session_id, session_obj_path);
}

static void logind_service_manager_dbus_connect(LogindService *self) {
    g_debug("logind_service.c:logind_service_manager_dbus_connect():");

    GError *error = NULL;

    DBUSService *dbus = dbus_service_get_global();
    self->conn = dbus_service_get_system_bus(dbus);

    self->manager = dbus_login1_manager_proxy_new_sync(
        self->conn, G_DBUS_PROXY_FLAGS_NONE, "org.freedesktop.login1",
        "/org/freedesktop/login1", NULL, &error);
    if (!self->manager)
        g_error(
            "logind_service.c:logind_service_manager_dbus_connect(): "
            "error: create manager proxy: %s",
            error->message);
}

static void on_idle_inhibitor_changed(GSettings *settings, gchar *key,
                                      LogindService *self) {
    g_debug("logind_service.c:on_idle_inhibitor_changed(): called");

    gboolean inhibit = g_settings_get_boolean(settings, "idle-inhibitor");
    logind_service_set_idle_inhibit(self, inhibit);
}

static void logind_service_init(LogindService *self) {
    g_debug("logind_service.c:logind_service_init():");

    logind_service_manager_dbus_connect(self);
    logind_service_session_dbus_connect(self);

    // connect to settings
    self->settings = g_settings_new("org.ldelossa.way-shell.system");

    // watch idle-inhibitor setting
    g_signal_connect(self->settings, "changed::idle-inhibitor",
                     G_CALLBACK(on_idle_inhibitor_changed), self);

    self->idle_inhibitor_fd = -1;
}

gboolean logind_service_can_reboot(LogindService *self) {
    g_debug("logind_service.c:logind_service_can_reboot():");

    GError *error = NULL;
    char *can_reboot;

    dbus_login1_manager_call_can_reboot_sync(self->manager, &can_reboot, NULL,
                                             &error);
    if (error) {
        g_critical("logind_service.c:logind_service_can_reboot(): error: %s",
                   error->message);
    }

    if (strcmp(can_reboot, "yes") == 0) {
        return true;
    }

    return false;
}

void logind_service_reboot(LogindService *self) {
    g_debug("logind_service.c:logind_service_reboot():");

    GError *error = NULL;

    dbus_login1_manager_call_reboot_sync(self->manager, false, NULL, &error);
    if (error) {
        g_critical("logind_service.c:logind_service_reboot(): error: %s",
                   error->message);
    }
}

gboolean logind_service_can_power_off(LogindService *self) {
    g_debug("logind_service.c:logind_service_can_power_off():");

    GError *error = NULL;
    char *can_power_off;

    dbus_login1_manager_call_can_power_off_sync(self->manager, &can_power_off,
                                                NULL, &error);
    if (error) {
        g_critical("logind_service.c:logind_service_can_power_off(): error: %s",
                   error->message);
    }

    if (strcmp(can_power_off, "yes") == 0) {
        return true;
    }

    return false;
}

void logind_service_power_off(LogindService *self) {
    g_debug("logind_service.c:logind_service_power_off():");

    GError *error = NULL;

    dbus_login1_manager_call_power_off_sync(self->manager, false, NULL, &error);
    if (error) {
        g_critical("logind_service.c:logind_service_power_off(): error: %s",
                   error->message);
    }
}

gboolean logind_service_can_suspend(LogindService *self) {
    g_debug("logind_service.c:logind_service_can_suspend():");

    GError *error = NULL;
    char *can_suspend;

    dbus_login1_manager_call_can_suspend_sync(self->manager, &can_suspend, NULL,
                                              &error);
    if (error) {
        g_critical("logind_service.c:logind_service_can_suspend(): error: %s",
                   error->message);
    }

    if (strcmp(can_suspend, "yes") == 0) {
        return true;
    }

    return false;
}

void logind_service_suspend(LogindService *self) {
    g_debug("logind_service.c:logind_service_suspend():");

    GError *error = NULL;

    dbus_login1_manager_call_suspend_sync(self->manager, false, NULL, &error);
    if (error) {
        g_critical("logind_service.c:logind_service_suspend(): error: %s",
                   error->message);
    }
}

gboolean logind_service_can_hibernate(LogindService *self) {
    g_debug("logind_service.c:logind_service_can_hibernate():");

    GError *error = NULL;
    char *can_hibernate;

    dbus_login1_manager_call_can_hibernate_sync(self->manager, &can_hibernate,
                                                NULL, &error);
    if (error) {
        g_critical("logind_service.c:logind_service_can_hibernate(): error: %s",
                   error->message);
    }

    if (strcmp(can_hibernate, "yes") == 0) {
        return true;
    }

    return false;
}

void logind_service_hibernate(LogindService *self) {
    g_debug("logind_service.c:logind_service_hibernate():");

    GError *error = NULL;

    dbus_login1_manager_call_hibernate_sync(self->manager, false, NULL, &error);
    if (error) {
        g_critical("logind_service.c:logind_service_hibernate(): error: %s",
                   error->message);
    }
}

gboolean logind_service_can_hybrid_sleep(LogindService *self) {
    g_debug("logind_service.c:logind_service_can_hybrid_sleep():");

    GError *error = NULL;
    char *can_hybrid_sleep;

    dbus_login1_manager_call_can_hybrid_sleep_sync(
        self->manager, &can_hybrid_sleep, NULL, &error);
    if (error) {
        g_critical(
            "logind_service.c:logind_service_can_hybrid_sleep(): error: %s",
            error->message);
    }

    if (strcmp(can_hybrid_sleep, "yes") == 0) {
        return true;
    }

    return false;
}

void logind_service_hybrid_sleep(LogindService *self) {
    g_debug("logind_service.c:logind_service_hybrid_sleep():");

    GError *error = NULL;

    dbus_login1_manager_call_hybrid_sleep_sync(self->manager, false, NULL,
                                               &error);
    if (error) {
        g_critical("logind_service.c:logind_service_hybrid_sleep(): error: %s",
                   error->message);
    }
}

gboolean logind_service_can_suspendthenhibernate(LogindService *self) {
    g_debug("logind_service.c:logind_service_can_suspendthenhibernate():");

    GError *error = NULL;
    char *can_suspend_then_hibernate;

    dbus_login1_manager_call_can_suspend_then_hibernate_sync(
        self->manager, &can_suspend_then_hibernate, NULL, &error);
    if (error) {
        g_critical(
            "logind_service.c:logind_service_can_suspendthenhibernate(): "
            "error: "
            "%s",
            error->message);
    }

    if (strcmp(can_suspend_then_hibernate, "yes") == 0) {
        return true;
    }

    return false;
}

void logind_service_suspendthenhibernate(LogindService *self) {
    g_debug("logind_service.c:logind_service_suspendthenhibernate():");

    GError *error = NULL;

    dbus_login1_manager_call_suspend_then_hibernate_sync(self->manager, false,
                                                         NULL, &error);
    if (error) {
        g_critical(
            "logind_service.c:logind_service_suspendthenhibernate(): error: %s",
            error->message);
    }
}

void logind_service_kill_session(LogindService *self) {
    g_debug("logind_service.c:logind_service_kill_session():");

    GError *error = NULL;

    dbus_login1_session_call_terminate_sync(self->session, NULL, &error);
    if (error) {
        g_critical("logind_service.c:logind_service_kill_session(): error: %s",
                   error->message);
    }
}

int logind_service_session_set_brightness(LogindService *self,
                                          const gchar *arg_subsystem,
                                          const gchar *arg_name,
                                          guint arg_brightness) {
    g_debug("logind_service.c:logind_service_session_set_brightness(): called");

    GError *error = NULL;
    dbus_login1_session_call_set_brightness_sync(
        self->session, arg_subsystem, arg_name, arg_brightness, NULL, &error);
    if (error) {
        g_critical(
            "logind_service.c:logind_service_session_set_brightness(): error: "
            "%s",
            error->message);
        return -1;
    }
    return 0;
}

gboolean logind_service_set_idle_inhibit(LogindService *self, gboolean enable) {
    g_debug(
        "logind_service.c:logind_service_set_idle_inhibit(): called. enable: "
        "%d idle_inhibitor_fd: %d",
        enable, self->idle_inhibitor_fd);

    if (enable && self->idle_inhibitor_fd != -1) return true;

    if (!enable && self->idle_inhibitor_fd == -1) return true;

    if (!enable) {
        g_debug(
            "logind_service.c:logind_service_set_idle_inhibit(): closing "
            "idle_inhibitor_fd: %d",
            self->idle_inhibitor_fd);

        if (close(self->idle_inhibitor_fd) != 0) {
            g_critical(
                "logind_service.c:logind_service_set_idle_inhibit(): error: "
                "close: %s",
                strerror(errno));
            return false;
        }
        self->idle_inhibitor_fd = -1;
        g_signal_emit(self, signals[idle_inhibitor_changed], 0, FALSE);

        return true;
    }

    GError *error = NULL;
    GVariant *result;
    GUnixFDList *fd_list = NULL;
    gint fd;
    gchar *what = "idle";
    gchar *who = "way-shell";
    gchar *why = "User initiated idle block";
    gchar *mode = "block";

    // Call the Inhibit method directly, for some reason the codegen does not
    // work :(
    result = g_dbus_connection_call_with_unix_fd_list_sync(
        self->conn,
        "org.freedesktop.login1",                       // service name
        "/org/freedesktop/login1",                      // object path
        "org.freedesktop.login1.Manager",               // interface name
        "Inhibit",                                      // method name
        g_variant_new("(ssss)", what, who, why, mode),  // parameters
        G_VARIANT_TYPE("(h)"),                          // response type
        G_DBUS_CALL_FLAGS_NONE,
        -1,        // timeout
        NULL,      // fd list for input
        &fd_list,  // fd list for output
        NULL,      // cancellable
        &error);

    // get fd
    g_variant_get(result, "(h)", &fd);

    self->idle_inhibitor_fd = g_unix_fd_list_get(fd_list, fd, NULL);

    g_object_unref(fd_list);
    g_variant_unref(result);

    g_debug("logind_service.c:logind_service_set_idle_inhibit(): fd: %d",
            self->idle_inhibitor_fd);

    g_signal_emit(self, signals[idle_inhibitor_changed], 0, TRUE);

    return true;
}

gboolean logind_service_get_idle_inhibit(LogindService *self) {
    g_debug("logind_service.c:logind_service_get_idle_inhibit(): called");
    return self->idle_inhibitor_fd != -1;
}

int logind_service_global_init(void) {
    g_debug("logind_service.c:logind_service_global_init():");

    if (global) return 0;

    global = g_object_new(LOGIND_SERVICE_TYPE, NULL);
    return 0;
}

LogindService *logind_service_get_global() {
    g_debug("logind_service.c:logind_service_get_global():");

    return global;
}
