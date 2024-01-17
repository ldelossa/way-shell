#include "panel_mediator.h"

#include "./message_tray/message_tray.h"
#include "./message_tray/message_tray_mediator.h"
#include "panel.h"
#include "quick_settings/quick_settings.h"
#include "quick_settings/quick_settings_mediator.h"

enum signals { signals_n };

struct _PanelMediator {
    GObject parent_instance;
};
static guint signals[signals_n] = {0};
G_DEFINE_TYPE(PanelMediator, panel_mediator, G_TYPE_OBJECT);

static void panel_mediator_dispose(GObject *gobject) {
    PanelMediator *self = PANEL_MEDIATOR(gobject);
    // Chain-up
    G_OBJECT_CLASS(panel_mediator_parent_class)->dispose(gobject);
};

static void panel_mediator_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(panel_mediator_parent_class)->finalize(gobject);
};

static void panel_mediator_class_init(PanelMediatorClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = panel_mediator_dispose;
    object_class->finalize = panel_mediator_finalize;
};

static void panel_mediator_init(PanelMediator *self){};

static void on_message_tray_visible(MessageTrayMediator *tray_mediator,
                                    MessageTray *tray, Panel *panel) {
    g_debug("panel_mediator.c:on_message_tray_visible() called.");
    panel_on_msg_tray_visible(panel);
}

static void on_message_tray_hidden(MessageTrayMediator *tray_mediator,
                                   MessageTray *tray, Panel *panel) {
    g_debug("panel_mediator.c:on_message_tray_hidden() called.");
    panel_on_msg_tray_hidden(panel);
}

static void on_quick_settings_visible(QuickSettingsMediator *qs_mediator,
                                      QuickSettings *qs, Panel *panel) {
    g_debug("panel_mediator.c:on_quick_settings_visible() called.");
    panel_on_qs_visible(panel);
}

static void on_quick_settings_hidden(QuickSettingsMediator *qs_mediator,
                                     QuickSettings *qs, Panel *panel) {
    g_debug("panel_mediator.c:on_quick_settings_hidden() called.");
    panel_on_qs_hidden(panel);
}

void panel_mediator_connect(PanelMediator *mediator) {
    MessageTrayMediator *msg_tray_mediator = message_tray_get_global_mediator();
    g_signal_connect(msg_tray_mediator, "message-tray-visible",
                     G_CALLBACK(on_message_tray_visible), mediator);
    g_signal_connect(msg_tray_mediator, "message-tray-hidden",
                     G_CALLBACK(on_message_tray_hidden), mediator);

    QuickSettingsMediator *qs_mediator = quick_settings_get_global_mediator();
    g_signal_connect(qs_mediator, "quick-settings-visible",
                     G_CALLBACK(on_quick_settings_visible), mediator);
    g_signal_connect(qs_mediator, "quick-settings-hidden",
                     G_CALLBACK(on_quick_settings_hidden), mediator);
};