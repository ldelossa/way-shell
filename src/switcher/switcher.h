#pragma once

#include <adwaita.h>

// Helper macro to derefernce an embedded Switcher.
#define SWITCHER(self) \
	self->switcher

// Switcher abstracts the common widget components necessary to build a
// on screen selector menu (think apps like rofi).
//
// The structure can be embedded into another struct or GOBject and the
// init method can be called to initialize the component.
//
// Other methods then exist for more customization.
// Because the struct is designed to be embedded, implementation of Switcher
// widgets have access and control over all components.
typedef struct _Switcher {
	// The main window that is created for the Switcher widget
    AdwWindow *win;
	// The main container for the Switcher widget.
    GtkBox *container;
	// Switcher widgets have their ListBox's scrolled after some defined max
	// size.
	GtkScrolledWindow *scrolled;
	// A list of contents within the switcher.
    GtkListBox *list;
	// The Search box which filters the items in list.
    GtkSearchEntry *search_entry;
	// A controller for keyboard shortcuts
    GtkEventController *key_controller;
} Switcher;

// Initialize a new Switcher widget.
//
// The has_list argument can be set to false to have a 'input only' Switcher that
// can use its search box just to obtain user's input.
void switcher_init(Switcher *self, gboolean has_list);

// Returns the top choice in the Switcher's list.
//
// For example, calling this after the user has entered a search term will
// return the closest matching item in the list.
GtkListBoxRow *switcher_top_choice(Switcher *self);
GtkListBoxRow *switcher_last_choice(Switcher *self);
