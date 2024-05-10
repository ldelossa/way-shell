#pragma once

#include <adwaita.h>

typedef struct _Panel Panel;

G_BEGIN_DECLS

// The mediator which aggregates and emits signals on behavior of Panels.
// This is useful since multiple Panels can exist, one per GdkMonitor.
// External components can connect to signals the PanelMediator emits rather
// then each Panel.
//
// PanelMediator also acts as a high-level signal router for cross cutting
// needs, such as closing the MessageTray component when the QuickSettings
// component is opened.
struct _PanelMediator;
#define PANEL_MEDIATOR_TYPE panel_mediator_get_type()
G_DECLARE_FINAL_TYPE(PanelMediator, panel_mediator, PANEL, MEDIATOR, GObject);

G_END_DECLS

// Emits the "notification_tray_toggle_request" signal for the give Panel.
void panel_mediator_emit_msg_tray_toggle_request(PanelMediator *pm,
                                                 Panel *panel);

// Emits the "quick_settings_toggle_request" signal for the give Panel.
void panel_mediator_emit_qs_toggle_request(PanelMediator *pm, Panel *panel);

// Connects the PanelMediator to all other Mediator's signals required.
void panel_mediator_connect(PanelMediator *mediator);
