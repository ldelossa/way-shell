#include <adwaita.h>

// QuickSettingsMenuWidget is a widget which defines the layout and sets the
// appropriate CSS such that all menus in the QuickSettings uses the same
// styling and layout.
//
// A GObject or another struct can embed this structure and initialize it.
// The GObject or other structure is then fre populate 'buttons_container' box
// with the selectable options in the menu.
typedef struct _QuickSettingsMenuWidget {
    // main container for the widget
    // css id: "#quick-settings-menu"
    GtkBox *container;
    // sub-container which holds a the title and list of options
    // css id: "#quick-settings-menu #options-container"
    GtkBox *options_container;
    // title box with an icon and a title
    // css id: "#quick-settings-menu #options-container #options-title"
    GtkBox *title_container;
    GtkLabel *title;
    // container that icon in title sits in
    // css id: "#quick-settings-menu #options-container #options-title
    // #icon-container"
    GtkBox *title_icon_container;
    // icon in container
    GtkImage *icon;
    // a vertical list of options within the menu.
    // css id: "#quick-settings-menu #options-container #options"
    GtkBox *options;
    // if 'scrolling' is set to true then the 'options' box will be wrapped
    // inside a scrolling window.
    GtkScrolledWindow *scroll;

} QuickSettingsMenuWidget;

void quick_settings_menu_widget_init(QuickSettingsMenuWidget *self,
                                     gboolean scrolling);

void quick_settings_menu_widget_set_title(QuickSettingsMenuWidget *self,
                                          const char *title);

void quick_settings_menu_widget_set_icon(QuickSettingsMenuWidget *self,
                                         const char *icon_name);

