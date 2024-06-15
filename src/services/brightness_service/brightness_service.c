
#include "brightness_service.h"

#include <adwaita.h>
#include <fcntl.h>
#include <sys/inotify.h>

#include "../logind_service/logind_service.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib-unix.h"

static BrightnessService *global = NULL;

enum signals { brightness_changed, keyboard_brightness_changed, signals_n };

struct _BrightnessService {
    // display brightness
    GObject parent_instance;
    guint32 backlight_brightness;
    guint32 max_backlight_brightness;
    GFile *backlight_device_path;
    GSettings *systems_settings;
    gboolean has_backlight_brightness;
    GFileMonitor *backlight_file_monitor;

    // keyboard brightness
    guint32 keyboard_brightness;
    guint32 keyboard_max_brightness;
    GFile *keyboard_device_path;
    gboolean has_keyboard_brightness;
    GFileMonitor *keyboard_file_monitor;
};
static guint signals[signals_n] = {0};
G_DEFINE_TYPE(BrightnessService, brightness_service, G_TYPE_OBJECT);

static float compute_brightness_percent(guint32 brightness,
                                        guint32 max_brightness) {
    return (float)brightness / (float)max_brightness;
}

static void brightness_service_dispose(GObject *object) {
    G_OBJECT_CLASS(brightness_service_parent_class)->dispose(object);
}
static void brightness_service_finalize(GObject *object) {
    BrightnessService *self = BRIGHTNESS_SERVICE(object);
    g_clear_object(&self->backlight_device_path);
    G_OBJECT_CLASS(brightness_service_parent_class)->finalize(object);
}
static void brightness_service_class_init(BrightnessServiceClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = brightness_service_dispose;
    object_class->finalize = brightness_service_finalize;

    signals[brightness_changed] = g_signal_new(
        "brightness-changed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_FLOAT);

    signals[keyboard_brightness_changed] = g_signal_new(
        "keyboard-brightness-changed", G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT);
}

static void get_current_backlight_brightness(BrightnessService *self) {
    gchar *brightness_str = NULL;

    GFile *brightness_file =
        g_file_get_child(self->backlight_device_path, "brightness");

    if (!g_file_load_contents(brightness_file, NULL, &brightness_str, NULL,
                              NULL, NULL)) {
        g_debug(
            "brightness_service.c:on_fd_write(): failed to read brightness "
            "file");
    }

    self->backlight_brightness = g_ascii_strtoull(brightness_str, NULL, 10);

    g_debug("brightness_service.c:on_fd_write(): brightness: %u",
            self->backlight_brightness);
}

static void get_current_keyboard_brightness(BrightnessService *self) {
    gchar *brightness_str = NULL;

    GFile *brightness_file =
        g_file_get_child(self->keyboard_device_path, "brightness");

    if (!g_file_load_contents(brightness_file, NULL, &brightness_str, NULL,
                              NULL, NULL)) {
        g_debug(
            "brightness_service.c:on_fd_write(): failed to read brightness "
            "file");
    }

    self->keyboard_brightness = g_ascii_strtoull(brightness_str, NULL, 10);

    g_debug("brightness_service.c:on_fd_write(): brightness: %u",
            self->keyboard_brightness);
}

void backlight_file_changed(GFileMonitor *file_mon, GFile *file,
                            GFile *other_file, GFileMonitorEvent event_type,
                            BrightnessService *self) {
    get_current_backlight_brightness(self);
    g_signal_emit(self, signals[brightness_changed], 0,
                  compute_brightness_percent(self->backlight_brightness,
                                             self->max_backlight_brightness));
}

static void on_backlight_directory_changed(GSettings *settings, gchar *key,
                                           gpointer user_data) {
    g_debug("brightness_service.c:on_backlight_directory_changed(): called");

    BrightnessService *self = user_data;
    g_clear_object(&self->backlight_device_path);

    if (self->backlight_file_monitor) {
        g_file_monitor_cancel(self->backlight_file_monitor);
        g_clear_object(&self->backlight_file_monitor);
    }

    // check for empty string.
    if (strlen(g_settings_get_string(settings, key)) == 0) {
        self->has_backlight_brightness = false;
        return;
    }

    // join the path /sys/class/backlight/ with the settings value and make
    // it a GFile*
    self->backlight_device_path = g_file_new_for_path(g_build_filename(
        "/sys/class/backlight", g_settings_get_string(settings, key), NULL));

    // debug device path
    g_debug(
        "brightness_service.c:on_backlight_directory_changed(): device path: "
        "%s",
        g_file_get_path(self->backlight_device_path));

    // ensure device path exists
    if (!g_file_query_exists(self->backlight_device_path, NULL)) {
        g_warning(
            "brightness_service.c:on_backlight_directory_changed(): backlight "
            "directory does not exist");
        g_clear_object(&self->backlight_device_path);
        self->has_backlight_brightness = false;
        return;
    }
    self->has_backlight_brightness = true;

    GFile *brightness_file =
        g_file_get_child(self->backlight_device_path, "brightness");
    g_debug(
        "brightness_service.c:on_backlight_directory_changed(): brightness "
        "file: "
        "%s",
        g_file_get_path(brightness_file));

    GFile *max_brightness_file =
        g_file_get_child(self->backlight_device_path, "max_brightness");
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
    self->max_backlight_brightness =
        g_ascii_strtoull(max_brightness_str, NULL, 10);

    get_current_backlight_brightness(self);

    // emit initial brightness event
    g_signal_emit(self, signals[brightness_changed], 0,
                  compute_brightness_percent(self->backlight_brightness,
                                             self->max_backlight_brightness));

    // setup file monitor
    error = NULL;
    self->backlight_file_monitor =
        g_file_monitor_file(self->backlight_device_path,
                            G_FILE_MONITOR_WATCH_HARD_LINKS, NULL, &error);
    if (error) {
        g_warning(
            "brightness_service.c:on_backlight_directory_changed(): failed to "
            "setup file monitor: %s",
            error->message);
        g_error_free(error);
        goto error;
    }

    g_signal_connect(self->backlight_file_monitor, "changed",
                     G_CALLBACK(backlight_file_changed), self);

    return;

error:
    g_clear_object(&self->backlight_device_path);
    g_clear_object(&brightness_file);
    g_clear_object(&max_brightness_file);
    return;
}

void keyboard_file_changed(GFileMonitor *file_mon, GFile *file,
                           GFile *other_file, GFileMonitorEvent event_type,
                           BrightnessService *self) {
    get_current_keyboard_brightness(self);
    g_signal_emit(self, signals[keyboard_brightness_changed], 0,
                  self->keyboard_brightness);
}

static void on_backlight_keyboard_changed(GSettings *settings, gchar *key,
                                          gpointer user_data) {
    g_debug("brightness_service.c:on_backlight_keyboard_changed(): called");

    BrightnessService *self = user_data;
    g_clear_object(&self->keyboard_device_path);

    if (self->keyboard_file_monitor) {
        g_file_monitor_cancel(self->keyboard_file_monitor);
        g_clear_object(&self->keyboard_file_monitor);
    }

    // check for empty string.
    if (strlen(g_settings_get_string(settings, key)) == 0) {
        self->has_keyboard_brightness = false;
        return;
    }

    // join the path /sys/class/leds/ with the settings value and make
    // it a GFile*
    self->keyboard_device_path = g_file_new_for_path(g_build_filename(
        "/sys/class/leds", g_settings_get_string(settings, key), NULL));

    // debug device path
    g_debug(
        "brightness_service.c:on_backlight_keyboard_changed(): device path: "
        "%s",
        g_file_get_path(self->keyboard_device_path));

    // ensure device path exists
    if (!g_file_query_exists(self->keyboard_device_path, NULL)) {
        g_warning(
            "brightness_service.c:on_backlight_keyboard_changed(): keyboard "
            "directory does not exist");
        g_clear_object(&self->keyboard_device_path);
        self->has_keyboard_brightness = false;
        return;
    }
    self->has_keyboard_brightness = true;

    GFile *brightness_file =
        g_file_get_child(self->keyboard_device_path, "brightness");
    g_debug(
        "brightness_service.c:on_backlight_keyboard_changed(): brightness"
        "file: "
        "%s",
        g_file_get_path(brightness_file));

    GFile *max_brightness_file =
        g_file_get_child(self->keyboard_device_path, "max_brightness");
    g_debug(
        "brightness_service.c:on_backlight_keyboard_changed(): max_brightness "
        "file: %s",
        g_file_get_path(max_brightness_file));

    // ensure brightness_file exists
    if (!g_file_query_exists(brightness_file, NULL)) {
        g_warning(
            "brightness_service.c:on_backlight_keyboard_changed(): "
            "brightness "
            "file does not exist");
        goto error;
    }

    // read max_brightness_file into variable
    GError *error = NULL;
    gchar *max_brightness_str = NULL;
    if (!g_file_load_contents(max_brightness_file, NULL, &max_brightness_str,
                              NULL, NULL, &error)) {
        g_warning(
            "brightness_service.c:on_backlight_keyboard_changed(): failed to "
            "read max_brightness file: %s",
            error->message);
        g_error_free(error);
        goto error;
    }
    self->keyboard_max_brightness =
        g_ascii_strtoull(max_brightness_str, NULL, 10);

    get_current_keyboard_brightness(self);

    // emit initial brightness event
    g_signal_emit(self, signals[keyboard_brightness_changed], 0,
                  self->keyboard_brightness);

    // setup file monitor
    error = NULL;
    self->keyboard_file_monitor =
        g_file_monitor_file(self->keyboard_device_path,
                            G_FILE_MONITOR_WATCH_HARD_LINKS, NULL, &error);
    if (error) {
        g_warning(
            "brightness_service.c:on_backlight_keyboard_changed(): failed to "
            "setup file monitor: %s",
            error->message);
        g_error_free(error);
        goto error;
    }

    g_signal_connect(self->keyboard_file_monitor, "changed",
                     G_CALLBACK(keyboard_file_changed), self);

    return;

error:
    g_clear_object(&self->keyboard_device_path);
    g_clear_object(&brightness_file);
    g_clear_object(&max_brightness_file);
    return;
}

static void brightness_service_init(BrightnessService *self) {
    self->systems_settings = g_settings_new("org.ldelossa.way-shell.system");
    self->has_backlight_brightness = false;
    self->has_keyboard_brightness = false;

    on_backlight_directory_changed(self->systems_settings,
                                   "backlight-directory", self);

    on_backlight_keyboard_changed(self->systems_settings,
                                  "keyboard-backlight-directory", self);

    // connect to setting's change.
    g_signal_connect(self->systems_settings, "changed::backlight-directory",
                     G_CALLBACK(on_backlight_directory_changed), self);
}

int brightness_service_global_init(void) {
    g_debug(
        "brightness_service.c:brightness_service_global_init(): "
        "initializing global service.");

    global = g_object_new(BRIGHTNESS_SERVICE_TYPE, NULL);

    return 0;
}

void brightness_service_backlight_up(BrightnessService *self) {
    LogindService *logind = logind_service_get_global();

    get_current_backlight_brightness(self);

    // add 1000 to current brightness
    self->backlight_brightness += 1000;

    // if brightness is over max brightness clamp it to max brightness
    if (self->backlight_brightness > self->max_backlight_brightness)
        self->backlight_brightness = self->max_backlight_brightness;

    // use logind service to set backlight
    if (logind_service_session_set_brightness(
            logind, "backlight",
            g_settings_get_string(self->systems_settings,
                                  "backlight-directory"),
            self->backlight_brightness) != 0)
        return;

    // file monitor will catch the change, update our internal state and
    // send a signal
}

void brightness_service_backlight_down(BrightnessService *self) {
    LogindService *logind = logind_service_get_global();

    get_current_backlight_brightness(self);

    gint32 brightness = self->backlight_brightness - 1000;

    // subtract 1000 from current brightness
    self->backlight_brightness -= 1000;

    // if brightness is under 0 clamp it to 0
    self->backlight_brightness = (brightness < 0) ? 0 : brightness;

    // use logind service to set backlight
    if (logind_service_session_set_brightness(
            logind, "backlight",
            g_settings_get_string(self->systems_settings,
                                  "backlight-directory"),
            self->backlight_brightness) != 0)
        return;

    // file monitor will catch the change, update our internal state and
    // send a signal
}

void brightness_service_set_backlight(BrightnessService *self, float percent) {
    LogindService *logind = logind_service_get_global();

    // clamp percent to 0.0 - 1.0
    percent = CLAMP(percent, 0.0, 1.0);

    // calculate new brightness
    guint32 brightness = (guint32)(self->max_backlight_brightness * percent);

    // use logind service to set backlight
    if (logind_service_session_set_brightness(
            logind, "backlight",
            g_settings_get_string(self->systems_settings,
                                  "backlight-directory"),
            brightness) != 0)
        return;

    // file monitor will catch the change, update our internal state and
    // send a signal
}

float brightness_service_get_backlight(BrightnessService *self) {
    return (float)self->backlight_brightness /
           (float)self->max_backlight_brightness;
}

gchar *brightness_service_map_icon(BrightnessService *self) {
    return "display-brightness-symbolic";
}

void brightness_service_keyboard_up(BrightnessService *self) {
    LogindService *logind = logind_service_get_global();

    get_current_keyboard_brightness(self);

    self->keyboard_brightness += 1;

    // if brightness exceeds max brightness, we actually want to turn off the
    // backlight. This works well for most modern laptops which only have a
    // single backlight button
    if (self->keyboard_brightness > self->keyboard_max_brightness)
        self->keyboard_brightness = 0;

    // use logind service to set backlight
    if (logind_service_session_set_brightness(
            logind, "leds",
            g_settings_get_string(self->systems_settings,
                                  "keyboard-backlight-directory"),
            self->keyboard_brightness) != 0)
        return;

    // file monitor will catch the change, update our internal state and
    // send a signal
}

void brightness_service_keyboard_down(BrightnessService *self) {
    LogindService *logind = logind_service_get_global();

    get_current_keyboard_brightness(self);

    gint32 brightness = self->keyboard_brightness - 1;

    // subtract 1000 from current brightness
    self->keyboard_brightness -= 1;

    // if brightness is under 0 then we actually want to turn the brightness to
    // max.
    // This works well for most modern laptops which only have a single
    // backlight button
    self->keyboard_brightness =
        (brightness < 0) ? self->keyboard_max_brightness : brightness;

    // use logind service to set backlight
    if (logind_service_session_set_brightness(
            logind, "leds",
            g_settings_get_string(self->systems_settings,
                                  "keyboard-backlight-directory"),
            self->keyboard_brightness) != 0)
        return;

    // file monitor will catch the change, update our internal state and
    // send a signal
}

void brightness_service_set_keyboard(BrightnessService *self, uint32_t value) {
    LogindService *logind = logind_service_get_global();

    // ensure value is not larger then max
    if (value > self->keyboard_max_brightness) return;

    // use logind service to set backlight
    if (logind_service_session_set_brightness(
            logind, "leds",
            g_settings_get_string(self->systems_settings,
                                  "keyboard-backlight-directory"),
            value) != 0)
        return;

    // file monitor will catch the change, update our internal state and
    // send a signal
}

uint32_t brightness_service_get_keyboard(BrightnessService *self) {
    return self->keyboard_brightness;
}

uint32_t brightness_service_get_keyboard_max(BrightnessService *self) {
    return self->keyboard_max_brightness;
}

BrightnessService *brightness_service_get_global() { return global; }

gboolean brightness_service_has_backlight_brightness(BrightnessService *self) {
    return self->has_backlight_brightness;
}

gboolean brightness_service_has_keyboard_brightness(BrightnessService *self) {
    return self->has_keyboard_brightness;
}
