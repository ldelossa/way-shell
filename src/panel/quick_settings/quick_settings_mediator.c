#include "quick_settings_mediator.h"

#include "../message_tray/message_tray.h"
#include "../panel.h"
#include "quick_settings.h"

enum signals {
    // the quick settings is about to be visible, but not on screen yet.
    quick_settings_will_show,
    // the quick settings is now visible.
    quick_settings_visible,
    // the quick settings is now hidden.
    quick_settings_hidden,
    signals_n
};

struct _QuickSettingsMediator {
    GObject parent_instance;
    QuickSettings *qs;
};
static guint panel_signals[signals_n] = {0};
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

    panel_signals[quick_settings_will_show] = g_signal_new(
        "quick-settings-will-show", G_TYPE_FROM_CLASS(object_class),
        G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2,
        QUICK_SETTINGS_TYPE, PANEL_TYPE);

    panel_signals[quick_settings_visible] =
        g_signal_new("quick-settings-visible", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2,
                     QUICK_SETTINGS_TYPE, PANEL_TYPE);

    panel_signals[quick_settings_hidden] =
        g_signal_new("quick-settings-hidden", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2,
                     QUICK_SETTINGS_TYPE, PANEL_TYPE);
};

static void quick_settings_mediator_init(QuickSettingsMediator *self){};

// Sets a pointer to the Tray this QuickSettingsMediator mediates signals for.
void quick_settings_mediator_set_tray(QuickSettingsMediator *mediator,
                                      QuickSettings *tray) {
    mediator->qs = tray;
};

// Emits the "quick-settings-will-show" signal.
void quick_settings_mediator_emit_will_show(QuickSettingsMediator *mediator,
                                            QuickSettings *tray, Panel *panel) {
    g_signal_emit(mediator, panel_signals[quick_settings_will_show], 0, tray,
                  panel);
};

// Emits the "quick-settings-visible" signal.
void quick_settings_mediator_emit_visible(QuickSettingsMediator *mediator,
                                          QuickSettings *tray, Panel *panel) {
    g_signal_emit(mediator, panel_signals[quick_settings_visible], 0, tray,
                  panel);
};

// Emits the "quick-settings-hidden" signal.
void quick_settings_mediator_emit_hidden(QuickSettingsMediator *mediator,
                                         QuickSettings *tray, Panel *panel) {
    g_signal_emit(mediator, panel_signals[quick_settings_hidden], 0, tray,
                  panel);
};

static void on_quick_settings_toggle_request(PanelMediator *panel_mediator,
                                             Panel *panel,
                                             QuickSettingsMediator *mediator) {
    g_debug("quick_settings_mediator.c:on_quick_settings_toggle_request() called.");
    quick_settings_toggle(mediator->qs, panel);
}

static void on_message_tray_will_show(MessageTrayMediator *msg_tray_mediator,
                                      MessageTray *tray,
                                      Panel *panel,
                                      QuickSettingsMediator *mediator) {
    g_debug("quick_settings_mediator.c:on_message_tray_will_show() hiding quick settings.");
    quick_settings_set_hidden(mediator->qs, panel);
}

void quick_settings_mediator_connect(QuickSettingsMediator *mediator) {
    PanelMediator *panel_mediator = panel_get_global_mediator();
    // toggle ourselves if the panel is requesting.
    g_signal_connect(panel_mediator, "quick-settings-toggle-request",
                     G_CALLBACK(on_quick_settings_toggle_request), mediator);

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