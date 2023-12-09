#include "message_tray_mediator.h"

#include "../panel.h"
#include "../quick_settings/quick_settings.h"
#include "message_tray.h"

enum signals {
    // the message tray is about to be visible, but not on screen yet.
    message_tray_will_show,
    // the message tray is now visible.
    message_tray_visible,
    // the message tray is now hidden.
    message_tray_hidden,
    signals_n
};

struct _MessageTrayMediator {
    GObject parent_instance;
    MessageTray *tray;
};
static guint panel_signals[signals_n] = {0};
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

    panel_signals[message_tray_will_show] =
        g_signal_new("message-tray-will-show", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2,
                     MESSAGE_TRAY_TYPE, PANEL_TYPE);

    panel_signals[message_tray_visible] =
        g_signal_new("message-tray-visible", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2,
                     MESSAGE_TRAY_TYPE, PANEL_TYPE);

    panel_signals[message_tray_hidden] =
        g_signal_new("message-tray-hidden", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2,
                     MESSAGE_TRAY_TYPE, PANEL_TYPE);
};

static void message_tray_mediator_init(MessageTrayMediator *self){};

// Emitted when the MessageTray is about to become visible.
// Includes the Panel which invoked its visibility.
void message_tray_mediator_emit_will_show(MessageTrayMediator *mediator,
                                          MessageTray *tray, Panel *panel) {
    g_debug(
        "message_tray_mediator.c:message_tray_mediator_emit_will_show() "
        "emitting signal.");
    g_signal_emit(mediator, panel_signals[message_tray_will_show], 0, tray,
                  panel);
};

// Emits the message-tray-visible signal on the MessageTrayMediator.
void message_tray_mediator_emit_visible(MessageTrayMediator *mediator,
                                        MessageTray *tray, Panel *panel) {
    g_debug(
        "message_tray_mediator.c:message_tray_mediator_emit_visible() emitting "
        "signal.");
    g_signal_emit(mediator, panel_signals[message_tray_visible], 0, tray,
                  panel);
};

// Emits the message-tray-hidden signal on the MessageTrayMediator.
void message_tray_mediator_emit_hidden(MessageTrayMediator *mediator,
                                       MessageTray *tray, Panel *panel) {
    g_debug(
        "message_tray_mediator.c:message_tray_mediator_emit_hidden() emitting "
        "signal.");
    g_signal_emit(mediator, panel_signals[message_tray_hidden], 0, tray, panel);
};

static void on_message_tray_toggle_request(PanelMediator *panel_mediator,
                                           Panel *panel,
                                           MessageTrayMediator *mediator) {
    g_debug("message_tray_mediator.c:on_message_tray_request() called.");
    message_tray_toggle(mediator->tray, panel);
}

static void on_quick_settings_will_show(QuickSettingsMediator *qs_mediator,
                                        QuickSettings *qs, Panel *panel,
                                        MessageTrayMediator *mediator) {
    g_debug("message_tray_mediator.c:on_quick_settings_will_show() called.");
    message_tray_set_hidden(mediator->tray, panel);
}

// Sets a pointer to the Tray this MessageTrayMediator mediates signals for.
void message_tray_mediator_set_tray(MessageTrayMediator *mediator,
                                    MessageTray *tray) {
    mediator->tray = tray;
};

void message_tray_mediator_connect(MessageTrayMediator *mediator) {
    // connect to Panel mediator's request to toggle message tray.
    PanelMediator *panel_mediator = panel_get_global_mediator();
    g_signal_connect(panel_mediator, "message-tray-toggle-request",
                     G_CALLBACK(on_message_tray_toggle_request), mediator);

    QuickSettingsMediator *quick_settings_mediator =
        quick_settings_get_global_mediator();
    g_signal_connect(quick_settings_mediator, "quick-settings-will-show",
                     G_CALLBACK(on_quick_settings_will_show), mediator);
}
