#pragma once

#include <adwaita.h>

typedef struct _Panel Panel;
typedef struct _QuickSettings QuickSettings;

G_BEGIN_DECLS

struct _QuickSettingsMediator;
#define QUICK_SETTINGS_MEDIATOR_TYPE quick_settings_mediator_get_type()
G_DECLARE_FINAL_TYPE(QuickSettingsMediator, quick_settings_mediator, QS,
                     MEDIATOR, GObject);

G_END_DECLS

void quick_settings_mediator_set_qs(QuickSettingsMediator *mediator,
                                    QuickSettings *qs);

void quick_settings_mediator_emit_will_show(QuickSettingsMediator *mediator,
                                            QuickSettings *qs, Panel *panel);

void quick_settings_mediator_emit_visible(QuickSettingsMediator *mediator,
                                          QuickSettings *qs, Panel *panel);

void quick_settings_mediator_emit_hidden(QuickSettingsMediator *mediator,
                                         QuickSettings *qs, Panel *panel);

void quick_settings_mediator_connect(QuickSettingsMediator *mediator);

// Emits the message-tray-open signal on the MessageTrayMediator.
// Tells the message tray to open on the provided monitor.
void quick_settings_mediator_req_open(QuickSettingsMediator *mediator,
                                      Panel *panel);

// Emits the message-tray-close signal on the MessageTrayMediator.
// Tells the message tray to close if its visible.
void quick_settings_mediator_req_close(QuickSettingsMediator *mediator);

void quick_settings_mediator_req_shrink(QuickSettingsMediator *mediator);