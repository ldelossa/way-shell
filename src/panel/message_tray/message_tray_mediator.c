#include "message_tray_mediator.h"

#include "../quick_settings/quick_settings.h"
#include "message_tray.h"

enum signals {
    // signals

    // the message tray is about to be visible, but not on screen yet.
    message_tray_will_show,
    // the message tray is now visible.
    message_tray_visible,
    // the message tray is about to be hidden, still on screen.
    message_tray_will_hide,
    // the message tray is now hidden.
    message_tray_hidden,

    // actions
    message_tray_open,
    message_tray_close,
    message_tray_shrink_req,
    signals_n
};

struct _MessageTrayMediator {
    GObject parent_instance;
    MessageTray *tray;
};
static guint signals[signals_n] = {0};
G_DEFINE_TYPE(MessageTrayMediator, message_tray_mediator, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init functions.
static void message_tray_mediator_dispose(GObject *gobject) {
    MessageTrayMediator *self = MESSAGE_TRAY_MEDIATOR(gobject);
    // Chain-up
    G_OBJECT_CLASS(message_tray_mediator_parent_class)->dispose(gobject);
};

static void message_tray_mediator_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(message_tray_mediator_parent_class)->finalize(gobject);
};

static void message_tray_mediator_class_init(MessageTrayMediatorClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = message_tray_mediator_dispose;
    object_class->finalize = message_tray_mediator_finalize;

    signals[message_tray_will_show] =
        g_signal_new("message-tray-will-show", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2,
                     MESSAGE_TRAY_TYPE, GDK_TYPE_MONITOR);

    signals[message_tray_visible] =
        g_signal_new("message-tray-visible", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2,
                     MESSAGE_TRAY_TYPE, GDK_TYPE_MONITOR);

    signals[message_tray_will_hide] =
        g_signal_new("message-tray-will-hide", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2,
                     MESSAGE_TRAY_TYPE, GDK_TYPE_MONITOR);

    signals[message_tray_hidden] =
        g_signal_new("message-tray-hidden", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2,
                     MESSAGE_TRAY_TYPE, GDK_TYPE_MONITOR);

    signals[message_tray_open] =
        g_signal_new("message-tray-open", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
                     GDK_TYPE_MONITOR);

    signals[message_tray_close] =
        g_signal_new("message-tray-close", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    signals[message_tray_shrink_req] =
        g_signal_new("message-tray-shrink", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
};

static void on_message_tray_open(MessageTrayMediator *mediator,
                                 GdkMonitor *monitor) {
    g_debug("message_tray_mediator.c:on_message_tray_open() called.");
    message_tray_set_visible(mediator->tray, monitor);
};

static void on_message_tray_close(MessageTrayMediator *mediator) {
    g_debug("message_tray_mediator.c:message_tray_close() called.");
    if (message_tray_get_monitor(mediator->tray) == NULL) {
        return;
    }
    message_tray_set_hidden(mediator->tray,
                            message_tray_get_monitor(mediator->tray));
};

static void on_message_tray_shrink(MessageTrayMediator *mediator) {
    g_debug("message_tray_mediator.c:message_tray_shrink() called.");
    if (message_tray_get_monitor(mediator->tray) == NULL) {
        return;
    }
    message_tray_shrink(mediator->tray);
};

static void message_tray_mediator_init(MessageTrayMediator *self) {
    // wire into our open and close signals
    g_signal_connect(self, "message-tray-open",
                     G_CALLBACK(on_message_tray_open), NULL);

    g_signal_connect(self, "message-tray-close",
                     G_CALLBACK(on_message_tray_close), NULL);

    g_signal_connect(self, "message-tray-shrink",
                     G_CALLBACK(on_message_tray_shrink), NULL);
};

// Emitted when the MessageTray is about to become visible.
// Includes the Panel which invoked its visibility.
void message_tray_mediator_emit_will_show(MessageTrayMediator *mediator,
                                          MessageTray *tray,
                                          GdkMonitor *monitor) {
    g_debug(
        "message_tray_mediator.c:message_tray_mediator_emit_will_show() "
        "emitting signal.");
    g_signal_emit(mediator, signals[message_tray_will_show], 0, tray, monitor);
};

// Emits the message-tray-visible signal on the MessageTrayMediator.
void message_tray_mediator_emit_visible(MessageTrayMediator *mediator,
                                        MessageTray *tray,
                                        GdkMonitor *monitor) {
    g_debug(
        "message_tray_mediator.c:message_tray_mediator_emit_visible() emitting "
        "signal.");
    g_signal_emit(mediator, signals[message_tray_visible], 0, tray, monitor);
};

// Emits the message-tray-will-hide signal on the MessageTrayMediator.
void message_tray_mediator_emit_will_hide(MessageTrayMediator *mediator,
                                          MessageTray *tray,
                                          GdkMonitor *monitor) {
    g_debug(
        "message_tray_mediator.c:message_tray_mediator_emit_will_hide() "
        "emitting signal.");
    g_signal_emit(mediator, signals[message_tray_will_hide], 0, tray, monitor);
};

// Emits the message-tray-hidden signal on the MessageTrayMediator.
void message_tray_mediator_emit_hidden(MessageTrayMediator *mediator,
                                       MessageTray *tray, GdkMonitor *monitor) {
    g_debug(
        "message_tray_mediator.c:message_tray_mediator_emit_hidden() emitting "
        "signal.");
    g_signal_emit(mediator, signals[message_tray_hidden], 0, tray, monitor);
};

// Emits the message-tray-open signal on the MessageTrayMediator.
void message_tray_mediator_req_open(MessageTrayMediator *mediator,
                                    GdkMonitor *monitor) {
    g_debug(
        "message_tray_mediator.c:message_tray_mediator_emit_open() emitting "
        "signal.");
    g_signal_emit(mediator, signals[message_tray_open], 0, monitor);
};

// Emits the message-tray-close signal on the MessageTrayMediator.
void message_tray_mediator_req_close(MessageTrayMediator *mediator) {
    g_debug(
        "message_tray_mediator.c:message_tray_mediator_emit_close() emitting "
        "signal.");
    g_signal_emit(mediator, signals[message_tray_close], 0);
};

void message_tray_mediator_req_shrink(MessageTrayMediator *mediator) {
    g_debug(
        "message_tray_mediator.c:message_tray_mediator_emit_close() emitting "
        "signal.");
    g_signal_emit(mediator, signals[message_tray_shrink_req], 0);
};

static void on_quick_settings_will_show(QuickSettingsMediator *qs_mediator,
                                        QuickSettings *qs, GdkMonitor *monitor,
                                        MessageTrayMediator *mediator) {
    g_debug("message_tray_mediator.c:on_quick_settings_will_show() called.");
    message_tray_set_hidden(mediator->tray, monitor);
}

// Sets a pointer to the Tray this MessageTrayMediator mediates signals for.
void message_tray_mediator_set_tray(MessageTrayMediator *mediator,
                                    MessageTray *tray) {
    mediator->tray = tray;
};

void message_tray_mediator_connect(MessageTrayMediator *mediator) {
    QuickSettingsMediator *quick_settings_mediator =
        quick_settings_get_global_mediator();
    g_signal_connect(quick_settings_mediator, "quick-settings-will-show",
                     G_CALLBACK(on_quick_settings_will_show), mediator);
}
