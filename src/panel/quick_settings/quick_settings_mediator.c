#include "quick_settings_mediator.h"

#include "../message_tray/message_tray.h"
#include "../panel.h"
#include "quick_settings.h"

enum signals {
    // signals ...
    // the quick settings is about to be visible, but not on screen yet.
    quick_settings_will_show,
    // the quick settings is now visible.
    quick_settings_visible,
    // the quick settings is now hidden.
    quick_settings_hidden,

    // actions ...
    quick_settings_req_open,
    quick_settings_req_close,
    quick_settings_req_shrink,

    signals_n
};

typedef struct _QuickSettingsMediator {
    GObject parent_instance;
    QuickSettings *qs;
} QuickSettingsMediator;
static guint signals[signals_n] = {0};
G_DEFINE_TYPE(QuickSettingsMediator, quick_settings_mediator, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init functions.
static void quick_settings_mediator_dispose(GObject *gobject) {
    QuickSettingsMediator *self = QS_MEDIATOR(gobject);
    // Chain-up
    G_OBJECT_CLASS(quick_settings_mediator_parent_class)->dispose(gobject);
};

static void quick_settings_mediator_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(quick_settings_mediator_parent_class)->finalize(gobject);
};

static void quick_settings_mediator_class_init(
    QuickSettingsMediatorClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = quick_settings_mediator_dispose;
    object_class->finalize = quick_settings_mediator_finalize;

    signals[quick_settings_will_show] = g_signal_new(
        "quick-settings-will-show", G_TYPE_FROM_CLASS(object_class),
        G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2,
        QUICK_SETTINGS_TYPE, PANEL_TYPE);

    signals[quick_settings_visible] =
        g_signal_new("quick-settings-visible", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2,
                     QUICK_SETTINGS_TYPE, PANEL_TYPE);

    signals[quick_settings_hidden] =
        g_signal_new("quick-settings-hidden", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2,
                     QUICK_SETTINGS_TYPE, PANEL_TYPE);

    signals[quick_settings_req_open] = g_signal_new(
        "quick-settings-req-open", G_TYPE_FROM_CLASS(object_class),
        G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, PANEL_TYPE);

    signals[quick_settings_req_close] = g_signal_new(
        "quick-settings-req-close", G_TYPE_FROM_CLASS(object_class),
        G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    signals[quick_settings_req_shrink] = g_signal_new(
        "quick-settings-req-shrink", G_TYPE_FROM_CLASS(object_class),
        G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
};

static void on_quick_settings_open(QuickSettingsMediator *mediator,
                                   Panel *panel) {
    g_debug("quick_settings_mediator.c:on_quick_settings_open() called.");
    quick_settings_set_visible(mediator->qs, panel);
};

static void on_quick_settings_close(QuickSettingsMediator *mediator) {
    g_debug("quick_settings_mediator.c:quick_settings_close() called.");
    if (quick_settings_get_panel(mediator->qs) == NULL) {
        return;
    }
    quick_settings_set_hidden(mediator->qs,
                              quick_settings_get_panel(mediator->qs));
};

static void on_quick_settings_shrink(QuickSettingsMediator *mediator) {
    g_debug("quick_settings_mediator.c:quick_settings_shrink() called.");
    if (quick_settings_get_panel(mediator->qs) == NULL) {
        return;
    }
    quick_settings_shrink(mediator->qs);
};

static void quick_settings_mediator_init(QuickSettingsMediator *self) {
    // write into out actions api
    g_signal_connect(self, "quick-settings-req-open",
                     G_CALLBACK(on_quick_settings_open), NULL);
    g_signal_connect(self, "quick-settings-req-close",
                     G_CALLBACK(on_quick_settings_close), NULL);
    g_signal_connect(self, "quick-settings-req-shrink",
                     G_CALLBACK(on_quick_settings_shrink), NULL);
};

// Sets a pointer to the Tray this QuickSettingsMediator mediates signals for.
void quick_settings_mediator_set_tray(QuickSettingsMediator *mediator,
                                      QuickSettings *tray) {
    mediator->qs = tray;
};

// Emits the "quick-settings-will-show" signal.
void quick_settings_mediator_emit_will_show(QuickSettingsMediator *mediator,
                                            QuickSettings *tray, Panel *panel) {
    g_signal_emit(mediator, signals[quick_settings_will_show], 0, tray, panel);
};

// Emits the "quick-settings-visible" signal.
void quick_settings_mediator_emit_visible(QuickSettingsMediator *mediator,
                                          QuickSettings *tray, Panel *panel) {
    g_signal_emit(mediator, signals[quick_settings_visible], 0, tray, panel);
};

// Emits the "quick-settings-hidden" signal.
void quick_settings_mediator_emit_hidden(QuickSettingsMediator *mediator,
                                         QuickSettings *tray, Panel *panel) {
    g_signal_emit(mediator, signals[quick_settings_hidden], 0, tray, panel);
};

// Emits the message-tray-open signal on the MessageTrayMediator.
void quick_settings_mediator_req_open(QuickSettingsMediator *mediator,
                                      Panel *panel) {
    g_debug(
        "message_tray_mediator.c:message_tray_mediator_emit_open() emitting "
        "signal.");
    g_signal_emit(mediator, signals[quick_settings_req_open], 0, panel);
};

// Emits the message-tray-close signal on the MessageTrayMediator.
void quick_settings_mediator_req_close(QuickSettingsMediator *mediator) {
    g_debug(
        "message_tray_mediator.c:message_tray_mediator_emit_close() emitting "
        "signal.");
    g_signal_emit(mediator, signals[quick_settings_req_close], 0);
};

void quick_settings_mediator_req_shrink(QuickSettingsMediator *mediator) {
    g_debug(
        "message_tray_mediator.c:message_tray_mediator_emit_close() emitting "
        "signal.");
    g_signal_emit(mediator, signals[quick_settings_req_shrink], 0);
};

static void on_message_tray_will_show(MessageTrayMediator *msg_tray_mediator,
                                      MessageTray *tray, Panel *panel,
                                      QuickSettingsMediator *mediator) {
    g_debug(
        "quick_settings_mediator.c:on_message_tray_will_show() hiding quick "
        "settings.");
    quick_settings_set_hidden(mediator->qs, panel);
}

void quick_settings_mediator_connect(QuickSettingsMediator *mediator) {
    MessageTrayMediator *message_tray_mediator =
        message_tray_get_global_mediator();
    // hide ourselves if message-tray is about to show.
    g_signal_connect(message_tray_mediator, "message-tray-will-show",
                     G_CALLBACK(on_message_tray_will_show), mediator);
}

void quick_settings_mediator_set_qs(QuickSettingsMediator *mediator,
                                    QuickSettings *qs) {
    mediator->qs = qs;
};