
#include "brightness_service.h"

#include <adwaita.h>
#include <fcntl.h>
#include <sys/inotify.h>

#include "../logind_service/logind_service.h"
#include "glib-unix.h"

static BrightnessService *global = NULL;

enum signals { brightness_changed, signals_n };

struct _BrightnessService {
    GObject parent_instance;
    guint32 brightness;
    guint32 max_brightness;
    GFile *device_path;
    GSettings *backlight_dir;
};
static guint signals[signals_n] = {0};
G_DEFINE_TYPE(BrightnessService, brightness_service, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init methods
static void brightness_service_dispose(GObject *object) {
    G_OBJECT_CLASS(brightness_service_parent_class)->dispose(object);
}
static void brightness_service_finalize(GObject *object) {
    BrightnessService *self = BRIGHTNESS_SERVICE(object);
    g_clear_object(&self->device_path);
    G_OBJECT_CLASS(brightness_service_parent_class)->finalize(object);
}
static void brightness_service_class_init(BrightnessServiceClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = brightness_service_dispose;
    object_class->finalize = brightness_service_finalize;

    signals[brightness_changed] = g_signal_new(
        "brightness-changed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT);
}

static void get_current_brightness(BrightnessService *self) {
    gchar *brightness_str = NULL;

    GFile *brightness_file = g_file_get_child(self->device_path, "brightness");

    if (!g_file_load_contents(brightness_file, NULL, &brightness_str, NULL,
                              NULL, NULL)) {
        g_debug(
            "brightness_service.c:on_fd_write(): failed to read brightness "
            "file");
    }

    self->brightness = g_ascii_strtoull(brightness_str, NULL, 10);

    g_debug("brightness_service.c:on_fd_write(): brightness: %u",
            self->brightness);
}

static void on_backlight_directory_changed(GSettings *settings, gchar *key,
                                           gpointer user_data) {
    g_debug("brightness_service.c:on_backlight_directory_changed(): called");

    BrightnessService *self = user_data;
    g_clear_object(&self->device_path);

    // join the path /sys/class/backlight/ with the settings value and make
    // it a GFile*
    self->device_path = g_file_new_for_path(g_build_filename(
        "/sys/class/backlight", g_settings_get_string(settings, key), NULL));

    // debug device path
    g_debug(
        "brightness_service.c:on_backlight_directory_changed(): device path: "
        "%s",
        g_file_get_path(self->device_path));

    // ensure device path exists
    if (!g_file_query_exists(self->device_path, NULL)) {
        g_warning(
            "brightness_service.c:on_backlight_directory_changed(): backlight "
            "directory does not exist");
        g_clear_object(&self->device_path);
        return;
    }

    GFile *brightness_file = g_file_get_child(self->device_path, "brightness");
    g_debug(
        "brightness_service.c:on_backlight_directory_changed(): brightness "
        "file: "
        "%s",
        g_file_get_path(brightness_file));

    GFile *max_brightness_file =
        g_file_get_child(self->device_path, "max_brightness");
    g_debug(
        "brightness_service.c:on_backlight_directory_changed(): max_brightness "
        "file: %s",
        g_file_get_path(max_brightness_file));

    // ensure brightness_file exists
    if (!g_file_query_exists(brightness_file, NULL)) {
        g_warning(
            "brightness_service.c:on_backlight_directory_changed(): brightness "
            "file does not exist");
        goto error;
    }

    // read max_brightness_file into variable
    GError *error = NULL;
    gchar *max_brightness_str = NULL;
    if (!g_file_load_contents(max_brightness_file, NULL, &max_brightness_str,
                              NULL, NULL, &error)) {
        g_warning(
            "brightness_service.c:on_backlight_directory_changed(): failed to "
            "read max_brightness file: %s",
            error->message);
        g_error_free(error);
        goto error;
    }
    self->max_brightness = g_ascii_strtoull(max_brightness_str, NULL, 10);

    // read current brightness into variable
    gchar *brightness_str = NULL;
    if (!g_file_load_contents(brightness_file, NULL, &brightness_str, NULL,
                              NULL, &error)) {
        g_warning(
            "brightness_service.c:on_backlight_directory_changed(): failed to "
            "read brightness file: %s",
            error->message);
        g_error_free(error);
        goto error;
    }
    self->brightness = g_ascii_strtoull(brightness_str, NULL, 10);

    get_current_brightness(self);

    // emit initial brightness event
    g_signal_emit(self, signals[brightness_changed], 0, self->brightness);

    return;

error:
    g_clear_object(&self->device_path);
    g_clear_object(&brightness_file);
    g_clear_object(&max_brightness_file);
    return;
}

static void brightness_service_init(BrightnessService *self) {
    self->backlight_dir = g_settings_new("org.ldelossa.way-shell.system");

    on_backlight_directory_changed(self->backlight_dir, "backlight-directory",
                                   self);

    // connect to setting's change.
    g_signal_connect(self->backlight_dir, "changed::backlight-directory",
                     G_CALLBACK(on_backlight_directory_changed), self);
}

int brightness_service_global_init(void) {
    g_debug(
        "brightness_service.c:brightness_service_global_init(): "
        "initializing global service.");

    global = g_object_new(BRIGHTNESS_SERVICE_TYPE, NULL);

    return 0;
}

void brightness_service_up(BrightnessService *self) {
    LogindService *logind = logind_service_get_global();

    get_current_brightness(self);

    // add 1000 to current brightness
    self->brightness += 1000;

    // if brightness is over max brightness clamp it to max brightness
    if (self->brightness > self->max_brightness)
        self->brightness = self->max_brightness;

    // use logind service to set backlight
    if (logind_service_session_set_brightness(
            logind, "backlight",
            g_settings_get_string(self->backlight_dir, "backlight-directory"),
            self->brightness) != 0)
        return;

    // send signal with new brightness
    g_signal_emit(self, signals[brightness_changed], 0, self->brightness);
}

void brightness_service_down(BrightnessService *self) {
    LogindService *logind = logind_service_get_global();

    get_current_brightness(self);

    // subtract 1000 from current brightness
    self->brightness -= 1000;

    // if brightness is under 0 clamp it to 0
    if (self->brightness < 0) self->brightness = 0;

    // use logind service to set backlight
    if (logind_service_session_set_brightness(
            logind, "backlight",
            g_settings_get_string(self->backlight_dir, "backlight-directory"),
            self->brightness) != 0)
        return;

    // send signal with new brightness
    g_signal_emit(self, signals[brightness_changed], 0, self->brightness);
}

BrightnessService *brightness_service_get_global() { return global; }