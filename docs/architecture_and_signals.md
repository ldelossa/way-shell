# Architecture And Signals

The major components of Gnomeland are Subsystems, Mediators, and Services.

A Subsystem is a major component of the Gnomeland UI and encapsulates all the
business logic to support its Widget heirarchy.

Separate Subsystems communicate with each other only via Mediators.
Mediators are signal bridges which simplify the sending and receiving of signals
between Subsystems.

Services provide functionality not associated with UI to the application.
These often provide access to external APIs or provide data only API endpoints
to support a Widget's properties.

## Subsystems

## Panel

The Panel subsystem is responsible for placing a panel to the top of each
discovered GdkMonitor discovered.
It will react to GdkMonitor removal or deletion and adjust the prescence of
panels accordingly.

### Panel Signals

The follar are signals which other Mediators will connect to.

| Signal | Signature | Description |
| --- | --- | --- |
| "message-tray-toggle-request" | void (*Panel) | A request to open or close the MessageTray widget relative to the given *Panel. |

## MessageTray

The MessageTray subsystem is an interface to the Notification service along with
providing a simple clock and calendar widget.

The MessageTray will display the current state of notifications encountered
during the user's session and allow the user to dismiss or activate them.

### MessageTray Signals

The follar are signals which other Mediators will connect to.

| Signal | Signature | Description |
| --- | --- | --- |
| "message-tray-visible" | void (*MessageTray) | The MessageTray is now visible |
| "message-tray-hidden" | void (*MessageTray) | The MessageTray is not invisible |

## Mediators

Mediators are structures which handle connecting to and emitting signals between
Subsystems.

All signals between Subsystems should use a Mediator.
Intra-Subsystem signals don't need this abstraction and the Subsystems are free
to handle signals between their internal components as they see fit.
