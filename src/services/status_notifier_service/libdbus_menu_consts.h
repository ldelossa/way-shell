/*
A library to communicate a menu object set accross DBus and
track updates and maintain consistency.

Copyright 2009 Canonical Ltd.

Authors:
    Ted Gould <ted@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of either or both of the following licenses:

1) the GNU Lesser General Public License version 3, as published by the 
Free Software Foundation; and/or
2) the GNU Lesser General Public License version 2.1, as published by 
the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY or FITNESS FOR A PARTICULAR 
PURPOSE.  See the applicable version of the GNU Lesser General Public 
License for more details.

You should have received a copy of both the GNU Lesser General Public 
License version 3 and version 2.1 along with this program.  If not, see 
<http://www.gnu.org/licenses/>
*/

#pragma once

/* ***************************************** */
/* *********  GLib Object Signals  ********* */
/* ***************************************** */
/**
 * DBUSMENU_MENUITEM_SIGNAL_PROPERTY_CHANGED:
 *
 * String to attach to signal #DbusmenuServer::property-changed
 */
#define DBUSMENU_MENUITEM_SIGNAL_PROPERTY_CHANGED    "property-changed"
/**
 * DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED:
 *
 * String to attach to signal #DbusmenuServer::item-activated
 */
#define DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED      "item-activated"
/**
 * DBUSMENU_MENUITEM_SIGNAL_CHILD_ADDED:
 *
 * String to attach to signal #DbusmenuServer::child-added
 */
#define DBUSMENU_MENUITEM_SIGNAL_CHILD_ADDED         "child-added"
/**
 * DBUSMENU_MENUITEM_SIGNAL_CHILD_REMOVED:
 *
 * String to attach to signal #DbusmenuServer::child-removed
 */
#define DBUSMENU_MENUITEM_SIGNAL_CHILD_REMOVED       "child-removed"
/**
 * DBUSMENU_MENUITEM_SIGNAL_CHILD_MOVED:
 *
 * String to attach to signal #DbusmenuServer::child-moved
 */
#define DBUSMENU_MENUITEM_SIGNAL_CHILD_MOVED         "child-moved"
/**
 * DBUSMENU_MENUITEM_SIGNAL_REALIZED:
 *
 * String to attach to signal #DbusmenuServer::realized
 */
#define DBUSMENU_MENUITEM_SIGNAL_REALIZED            "realized"
/**
 * DBUSMENU_MENUITEM_SIGNAL_REALIZED_ID:
 *
 * ID to attach to signal #DbusmenuServer::realized
 */
#define DBUSMENU_MENUITEM_SIGNAL_REALIZED_ID         (g_signal_lookup(DBUSMENU_MENUITEM_SIGNAL_REALIZED, DBUSMENU_TYPE_MENUITEM))
/**
 * DBUSMENU_MENUITEM_SIGNAL_SHOW_TO_USER:
 *
 * String to attach to signal #DbusmenuServer::show-to-user
 */
#define DBUSMENU_MENUITEM_SIGNAL_SHOW_TO_USER        "show-to-user"
/**
 * DBUSMENU_MENUITEM_SIGNAL_ABOUT_TO_SHOW:
 *
 * String to attach to signal #DbusmenuServer::about-to-show
 */
#define DBUSMENU_MENUITEM_SIGNAL_ABOUT_TO_SHOW       "about-to-show"
/**
 * DBUSMENU_MENUITEM_SIGNAL_EVENT:
 *
 * String to attach to signal #DbusmenuServer::event
 */
#define DBUSMENU_MENUITEM_SIGNAL_EVENT               "event"

/* ***************************************** */
/* *********  Menuitem Properties  ********* */
/* ***************************************** */
/**
 * DBUSMENU_MENUITEM_PROP_TYPE:
 *
 * #DbusmenuMenuitem property used to represent what type of menuitem
 * this object represents.  Type: #G_VARIANT_TYPE_STRING.
 */
#define DBUSMENU_MENUITEM_PROP_TYPE                  "type"
/**
 * DBUSMENU_MENUITEM_PROP_VISIBLE:
 *
 * #DbusmenuMenuitem property used to represent whether the menuitem
 * should be shown or not.  Type: #G_VARIANT_TYPE_BOOLEAN.
 */
#define DBUSMENU_MENUITEM_PROP_VISIBLE               "visible"
/**
 * DBUSMENU_MENUITEM_PROP_ENABLED:
 *
 * #DbusmenuMenuitem property used to represent whether the menuitem
 * is clickable or not.  Type: #G_VARIANT_TYPE_BOOLEAN.
 */
#define DBUSMENU_MENUITEM_PROP_ENABLED               "enabled"
/**
 * DBUSMENU_MENUITEM_PROP_LABEL:
 *
 * #DbusmenuMenuitem property used for the text on the menu item.
 * Type: #G_VARIANT_TYPE_STRING
 */
#define DBUSMENU_MENUITEM_PROP_LABEL                 "label"
/**
 * DBUSMENU_MENUITEM_PROP_ICON_NAME:
 *
 * #DbusmenuMenuitem property that is the name of the icon under the
 * Freedesktop.org icon naming spec.  Type: #G_VARIANT_TYPE_STRING
 */
#define DBUSMENU_MENUITEM_PROP_ICON_NAME             "icon-name"
/**
 * DBUSMENU_MENUITEM_PROP_ICON_DATA:
 *
 * #DbusmenuMenuitem property that is the raw data of a custom icon
 * used in the application.  Type: #G_VARIANT_TYPE_VARIANT
 *
 * It is recommended that this is not set directly but instead the
 * libdbusmenu-gtk library is used with the function dbusmenu_menuitem_property_set_image()
 */
#define DBUSMENU_MENUITEM_PROP_ICON_DATA             "icon-data"
/**
 * DBUSMENU_MENUITEM_PROP_ACCESSIBLE_DESC:
 *
 * #DbusmenuMenuitem property used to provide a textual description of any
 * information that the icon may convey. The contents of this property are
 * passed through to assistive technologies such as the Orca screen reader.
 * The contents of this property will not be visible in the menu item. If
 * this property is set, Orca will use this property instead of the label 
 * property.
 * Type: #G_VARIANT_TYPE_STRING
 */
#define DBUSMENU_MENUITEM_PROP_ACCESSIBLE_DESC       "accessible-desc"
/**
 * DBUSMENU_MENUITEM_PROP_TOGGLE_TYPE:
 *
 * #DbusmenuMenuitem property that says what type of toggle entry should
 * be shown in the menu.  Should be either #DBUSMENU_MENUITEM_TOGGLE_CHECK
 * or #DBUSMENU_MENUITEM_TOGGLE_RADIO.  Type: #G_VARIANT_TYPE_STRING
 */
#define DBUSMENU_MENUITEM_PROP_TOGGLE_TYPE           "toggle-type"
/**
 * DBUSMENU_MENUITEM_PROP_TOGGLE_STATE:
 *
 * #DbusmenuMenuitem property that says what state a toggle entry should
 * be shown as the menu.  Should be either #DBUSMENU_MENUITEM_TOGGLE_STATE_UNCHECKED
 * #DBUSMENU_MENUITEM_TOGGLE_STATE_CHECKED or #DBUSMENU_MENUITEM_TOGGLE_STATUE_UNKNOWN.
 * Type: #G_VARIANT_TYPE_INT32
 */
#define DBUSMENU_MENUITEM_PROP_TOGGLE_STATE          "toggle-state"
/**
 * DBUSMENU_MENUITEM_PROP_SHORTCUT:
 *
 * #DbusmenuMenuitem property that is the entries that represent a shortcut
 * to activate the menuitem.  It is an array of arrays of strings.
 * Type: #G_VARIANT_TYPE_ARRAY
 *
 * It is recommended that this is not set directly but instead the
 * libdbusmenu-gtk library is used with the function dbusmenu_menuitem_property_set_shortcut()
 */
#define DBUSMENU_MENUITEM_PROP_SHORTCUT              "shortcut"
/**
 * DBUSMENU_MENUITEM_PROP_CHILD_DISPLAY:
 *
 * #DbusmenuMenuitem property that tells how the children of this menuitem
 * should be displayed.  Most likely this will be unset or of the value
 * #DBUSMENU_MENUITEM_CHILD_DISPLAY_SUBMENU.  Type: #G_VARIANT_TYPE_STRING
 */
#define DBUSMENU_MENUITEM_PROP_CHILD_DISPLAY         "children-display"
/**
 * DBUSMENU_MENUITEM_PROP_DISPOSITION:
 *
 * #DbusmenuMenuitem property to tell what type of information that the
 * menu item is displaying to the user.  Type: #G_VARIANT_TYPE_STRING
 */
#define DBUSMENU_MENUITEM_PROP_DISPOSITION           "disposition"

/* ***************************************** */
/* *********    Toggle Values      ********* */
/* ***************************************** */
/**
 * DBUSMENU_MENUITEM_TOGGLE_CHECK:
 *
 * Used to set #DBUSMENU_MENUITEM_PROP_TOGGLE_TYPE to be a standard
 * check mark item.
 */
#define DBUSMENU_MENUITEM_TOGGLE_CHECK               "checkmark"
/**
 * DBUSMENU_MENUITEM_TOGGLE_RADIO:
 *
 * Used to set #DBUSMENU_MENUITEM_PROP_TOGGLE_TYPE to be a standard
 * radio item.
 */
#define DBUSMENU_MENUITEM_TOGGLE_RADIO               "radio"

/* ***************************************** */
/* *********    Toggle States      ********* */
/* ***************************************** */
/**
 * DBUSMENU_MENUITEM_TOGGLE_STATE_UNCHECKED:
 *
 * Used to set #DBUSMENU_MENUITEM_PROP_TOGGLE_STATE so that the menu's
 * toggle item is empty.
 */
#define DBUSMENU_MENUITEM_TOGGLE_STATE_UNCHECKED     0
/**
 * DBUSMENU_MENUITEM_TOGGLE_STATE_CHECKED:
 *
 * Used to set #DBUSMENU_MENUITEM_PROP_TOGGLE_STATE so that the menu's
 * toggle item is filled.
 */
#define DBUSMENU_MENUITEM_TOGGLE_STATE_CHECKED       1
/**
 * DBUSMENU_MENUITEM_TOGGLE_STATE_UNKNOWN:
 *
 * Used to set #DBUSMENU_MENUITEM_PROP_TOGGLE_STATE so that the menu's
 * toggle item is undecided.
 */
#define DBUSMENU_MENUITEM_TOGGLE_STATE_UNKNOWN       -1

/* ***************************************** */
/* *********    Icon specials      ********* */
/* ***************************************** */
/**
 * DBUSMENU_MENUITEM_ICON_NAME_BLANK:
 *
 * Used to set #DBUSMENU_MENUITEM_PROP_TOGGLE_STATE so that the menu's
 * toggle item is undecided.
 */
#define DBUSMENU_MENUITEM_ICON_NAME_BLANK            "blank-icon"

/* ***************************************** */
/* *********  Shortcut Modifiers   ********* */
/* ***************************************** */
/**
 * DBUSMENU_MENUITEM_SHORTCUT_CONTROL:
 *
 * Used in #DBUSMENU_MENUITEM_PROP_SHORTCUT to represent the
 * control key.
 */
#define DBUSMENU_MENUITEM_SHORTCUT_CONTROL           "Control"
/**
 * DBUSMENU_MENUITEM_SHORTCUT_ALT:
 *
 * Used in #DBUSMENU_MENUITEM_PROP_SHORTCUT to represent the
 * alternate key.
 */
#define DBUSMENU_MENUITEM_SHORTCUT_ALT               "Alt"
/**
 * DBUSMENU_MENUITEM_SHORTCUT_SHIFT:
 *
 * Used in #DBUSMENU_MENUITEM_PROP_SHORTCUT to represent the
 * shift key.
 */
#define DBUSMENU_MENUITEM_SHORTCUT_SHIFT             "Shift"
/**
 * DBUSMENU_MENUITEM_SHORTCUT_SUPER:
 *
 * Used in #DBUSMENU_MENUITEM_PROP_SHORTCUT to represent the
 * super key.
 */
#define DBUSMENU_MENUITEM_SHORTCUT_SUPER             "Super"

/* ***************************************** */
/* *********  Child Display Types  ********* */
/* ***************************************** */
/**
 * DBUSMENU_MENUITEM_CHILD_DISPLAY_SUBMENU:
 *
 * Used in #DBUSMENU_MENUITEM_PROP_CHILD_DISPLAY to have the
 * subitems displayed as a submenu.
 */
#define DBUSMENU_MENUITEM_CHILD_DISPLAY_SUBMENU      "submenu"

/* ***************************************** */
/* ********* Menuitem Dispositions ********* */
/* ***************************************** */
/**
 * DBUSMENU_MENUITEM_DISPOSITION_NORMAL:
 *
 * Used in #DBUSMENU_MENUITEM_PROP_DISPOSITION to have a menu
 * item displayed in the normal manner.  Default value.
 */
#define DBUSMENU_MENUITEM_DISPOSITION_NORMAL         "normal"
/**
 * DBUSMENU_MENUITEM_DISPOSITION_INFORMATIVE:
 *
 * Used in #DBUSMENU_MENUITEM_PROP_DISPOSITION to have a menu
 * item displayed in a way that conveys it's giving additional
 * information to the user.
 */
#define DBUSMENU_MENUITEM_DISPOSITION_INFORMATIVE    "informative"
/**
 * DBUSMENU_MENUITEM_DISPOSITION_WARNING:
 *
 * Used in #DBUSMENU_MENUITEM_PROP_DISPOSITION to have a menu
 * item displayed in a way that conveys it's giving a warning
 * to the user.
 */
#define DBUSMENU_MENUITEM_DISPOSITION_WARNING        "warning"
/**
 * DBUSMENU_MENUITEM_DISPOSITION_ALERT:
 *
 * Used in #DBUSMENU_MENUITEM_PROP_DISPOSITION to have a menu
 * item displayed in a way that conveys it's giving an alert
 * to the user.
 */
#define DBUSMENU_MENUITEM_DISPOSITION_ALERT          "alert"

/* ***************************************** */
/* *********   Dbusmenu Events     ********* */
/* ***************************************** */
/**
 * DBUSMENU_MENUITEM_EVENT_ACTIVATED:
 *
 * String for the event identifier when a menu item is clicked
 * on by the user.
 */
#define DBUSMENU_MENUITEM_EVENT_ACTIVATED            "clicked"

/**
 * DBUSMENU_MENUITEM_EVENT_OPENED:
 *
 * String for the event identifier when a menu is opened and
 * displayed to the user.  Only valid for items that contain
 * submenus.
 */
#define DBUSMENU_MENUITEM_EVENT_OPENED               "opened"

/**
 * DBUSMENU_MENUITEM_EVENT_CLOSED:
 *
 * String for the event identifier when a menu is closed and
 * displayed to the user.  Only valid for items that contain
 * submenus.
 */
#define DBUSMENU_MENUITEM_EVENT_CLOSED               "closed"
