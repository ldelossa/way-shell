#include "logind_service.h"

#include <adwaita.h>

#include "logind_manager_dbus.h"
#include "logind_session_dbus.h"

static LogindService *global = NULL;

enum signals { signals_n };

struct _LogindService {
    GObject parent_instance;
    DbusLogin1Manager *manager;
    DbusLogin1Session *session;
    GDBusConnection *conn;
    const char *active_profile;
    GArray *profiles;
    gboolean enabled;
};
static guint signals[signals_n] = {0};
G_DEFINE_TYPE(LogindService, logind_service, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init methods
static void logind_service_dispose(GObject *gobject) {
    LogindService *self = LOGIND_SERVICE(gobject);

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
};

static void logind_service_session_dbus_connect(LogindService *self) {
    g_debug("logind_service.c:logind_service_session_dbus_connect():");

    int uid = getuid();

    GVariant *sessions = NULL;
    GError *error = NULL;
    char *session_obj_path = NULL;
    char *session_id = 0;

    dbus_login1_manager_call_list_sessions_sync(self->manager, &sessions, NULL,
                                                &error);
    if (error) {
        g_error(
            "logind_service.c:logind_service_init(): error: list sessions: %s",
            error->message);
    }

    GVariant *session = NULL;
    GVariantIter *iter;
    g_variant_get(sessions, "a(susso)", &iter);
    while (g_variant_iter_loop(iter, "@(susso)", &session)) {
        gchar *id;
        guint32 sess_uid;
        gchar *seat;
        gchar *display;
        gchar *obj_path;
        g_variant_get(session, "(susso)", &id, &sess_uid, &seat, &display,
                      &obj_path);
        if (sess_uid == uid) {
            session_obj_path = strdup(obj_path);
            session_id = strdup(id);
        }
    }
    g_variant_unref(sessions);
    g_variant_iter_free(iter);

    if (!session_obj_path)
        g_error(
            "logind_service.c:logind_service_init(): error: no session found");

    g_debug(
        "logind_service.c:logind_service_init(): found session id: %s "
        "session_obj_path: %s",
        session_id, session_obj_path);

    self->session = dbus_login1_session_proxy_new_sync(
        self->conn, G_DBUS_PROXY_FLAGS_NONE, "org.freedesktop.login1",
        session_obj_path, NULL, &error);
    if (!self->session)
        g_error(
            "logind_service.c:logind_service_init(): failed to create session "
            "proxy: %s",
            error->message);
    g_debug(
        "logind_service.c:logind_service_init(): session proxy created for "
        "user %d",
        uid);
}

static void logind_service_manager_dbus_connect(LogindService *self) {
    g_debug("logind_service.c:logind_service_manager_dbus_connect():");

    GError *error = NULL;

    self->conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!self->conn)
        g_error(
            "logind_service.c:logind_service_manager_dbus_connect(): "
            "error: connect to dbus: %s",
            error->message);

    self->manager = dbus_login1_manager_proxy_new_sync(
        self->conn, G_DBUS_PROXY_FLAGS_NONE, "org.freedesktop.login1",
        "/org/freedesktop/login1", NULL, &error);
    if (!self->manager)
        g_error(
            "logind_service.c:logind_service_manager_dbus_connect(): "
            "error: create manager proxy: %s",
            error->message);
}

static void logind_service_init(LogindService *self) {
    g_debug("logind_service.c:logind_service_init():");

    logind_service_manager_dbus_connect(self);
    logind_service_session_dbus_connect(self);
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