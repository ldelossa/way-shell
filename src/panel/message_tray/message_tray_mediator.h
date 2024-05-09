#pragma once

#include <adwaita.h>

typedef struct _Panel Panel;
typedef struct _MessageTray MessageTray;

G_BEGIN_DECLS

struct _MessageTrayMediator;
#define MESSAGE_TRAY_MEDIATOR_TYPE message_tray_mediator_get_type()
G_DECLARE_FINAL_TYPE(MessageTrayMediator, message_tray_mediator, MESSAGE_TRAY,
                     MEDIATOR, GObject);

G_END_DECLS

// Sets a pointer to the Tray this MessageTrayMediator mediates signals for.
void message_tray_mediator_set_tray(MessageTrayMediator *mediator,
                                    MessageTray *tray);

// Emitted when the MessageTray is about to become visible.
// Includes the GdkMonitor which invoked its visibility.
void message_tray_mediator_emit_will_show(MessageTrayMediator *mediator,
                                          MessageTray *tray,
                                          GdkMonitor *monitor);

// Emitted when the MessageTray has become visible.
// Includes the GdkMonitor which invoked its visibility.
void message_tray_mediator_emit_visible(MessageTrayMediator *mediator,
                                        MessageTray *tray, GdkMonitor *monitor);

// Emitted when the MessageTray is hidden.
// And the last GdkMonitor which invoked it.
void message_tray_mediator_emit_hidden(MessageTrayMediator *mediator,
                                       MessageTray *tray, GdkMonitor *monitor);

// Emitted when the MessageTray is about to be hidden.
// And the last GdkMonitor which invoked it.
void message_tray_mediator_emit_will_hide(MessageTrayMediator *mediator,
                                          MessageTray *tray,
                                          GdkMonitor *monitor);

// Connects the MessageTrayMediator to all other Mediator's signals
// required.
void message_tray_mediator_connect(MessageTrayMediator *mediator);

// Emits the message-tray-open signal on the MessageTrayMediator.
// Tells the message tray to open on the provided monitor.
void message_tray_mediator_req_open(MessageTrayMediator *mediator,
                                    GdkMonitor *monitor);

// Emits the message-tray-close signal on the MessageTrayMediator.
// Tells the message tray to close if its visible.
void message_tray_mediator_req_close(MessageTrayMediator *mediator);

void message_tray_mediator_req_shrink(MessageTrayMediator *mediator);
