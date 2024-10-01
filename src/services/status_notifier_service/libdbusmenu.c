#include <adwaita.h>
#include <gio/gio.h>

#include "libdbus_menu_consts.h"
#include "status_notifier_service.h"

gboolean libdbusmenu_parse_properties(GMenuItem *item, gchar *prop,
                                      GVariant *value, gboolean *is_separator,
                                      gboolean *is_visible) {
    // Currently, we use a GtkPopover to display the GMenu structure we are
    // creating.
    //
    // In Gtk4 the GtkPopover is very opinionated. It does not:
    // 1. Display icons
    // 2. Show states (like a checkbox or radio button)
    // 3. A way to disable the item without disabling the (global) GAction which
    //    handles the event.
    // 4. Does not show tool tips, so no good place for accessiblity string,
    // 	  not that this was a good usage for it anyway... but Menu APIs tend to
    // 	  just use that to add more info about the menu item.
    //
    // Therefore, we really just care about the label and whether this GMenuItem
    // is a separator or not and if its visible.
    if (g_strcmp0(prop, DBUSMENU_MENUITEM_PROP_TYPE) == 0) {
        // Type: #G_VARIANT_TYPE_STRING.
        g_debug(
            "status_notifier_service.c:libdbusmenu_parse_properties() setting "
            "label to %s",
            g_variant_get_string(value, NULL));
        if (g_strcmp0(g_variant_get_string(value, NULL), "separator") == 0) {
            *is_separator = true;
            return true;
        }
    }
    if (g_strcmp0(prop, DBUSMENU_MENUITEM_PROP_VISIBLE) == 0) {
        // Type: #G_VARIANT_TYPE_BOOLEAN.
        g_debug(
            "status_notifier_service.c:libdbusmenu_parse_properties() setting "
            "visible to %s",
            g_variant_get_boolean(value) ? "true" : "false");
        *is_visible = g_variant_get_boolean(value);
    }
    if (g_strcmp0(prop, DBUSMENU_MENUITEM_PROP_LABEL) == 0) {
        // Type: #G_VARIANT_TYPE_STRING.
        g_debug(
            "status_notifier_service.c:libdbusmenu_parse_properties() setting "
            "label to %s",
            g_variant_get_string(value, NULL));
        g_menu_item_set_label(item,
                              g_strdup(g_variant_get_string(value, NULL)));
    }
    return false;
}

// A recursive algorithm for parsing a root com.canonical.Menu structure.
// The algorithm works through the tree, turning Layout structures into GMenu
// and GMenuItems.
//
// When the function returns item->menu_model will be initialized if a menu has
// been parsed, of set to NULL if no menu existed for the layout.
void libdbusmenu_parse_layout(GVariant *layout, GMenuItem *parent_menu_item,
                              StatusNotifierItem *item) {
    GVariant *idv = g_variant_get_child_value(layout, 0);
    gint id = g_variant_get_int32(idv);
    g_variant_unref(idv);
    if (id < 0) {
        return;
    }
    g_debug(
        "status_notifier_service.c:libdbusmenu_parse_layout() handling menu "
        "id: %d",
        id);

    // TODO: Make this much better, this will result in menu flickers when
    // DBUS updates come in, we should incrementally parse updates...
    // ---
    // If we don't have a parent menu, this is the root iteration of a layout
    // parse.
    //
    // If the sni already has a menu, clear it out, calling code should signal
    // that a menu update occurred to references, and they can reload their
    // models into the appropriate GtkWidgets for display.
    GMenu *menu = NULL;
    if (!parent_menu_item && item->menu_model) {
        g_clear_object(&item->menu_model);
    }
    menu = g_menu_new();

    GVariantIter children;
    GVariant *children_variant;

    children_variant = g_variant_get_child_value(layout, 2);
    g_variant_iter_init(&children, children_variant);

    GMenu *current_section = NULL;
    GVariant *child;
    guint position = 0;
    while ((child = g_variant_iter_next_value(&children)) != NULL) {
        if (g_variant_is_of_type(child, G_VARIANT_TYPE_VARIANT)) {
            GVariant *tmp = g_variant_get_variant(child);
            g_variant_unref(child);
            child = tmp;
        }

        GVariant *child_id_variant = g_variant_get_child_value(child, 0);
        gint child_id = g_variant_get_int32(child_id_variant);
        g_variant_unref(child_id_variant);
        if (child_id < 0) {
            // Don't increment the position when there isn't a valid
            // node in the XML tree.  It's probably a comment.
            g_variant_unref(child);
            continue;
        }

        g_debug(
            "status_notifier_service.c:libdbusmenu_parse_layout() creating menu"
            "item on node %d at position %d child %d",
            id, position, child_id);

        GVariantIter iter;
        gchar *prop;
        GVariant *value;
        GVariant *child_props;

        child_props = g_variant_get_child_value(child, 1);
        g_variant_iter_init(&iter, child_props);

        // create a new menu item for this child and set its properties
        GMenuItem *menu_item = g_menu_item_new(
            NULL, "app.status_notifier_service-menu_item_clicked");

        gboolean is_section = false;
        gboolean is_visible = true;
        g_variant_iter_init(&iter, child_props);
        while (g_variant_iter_loop(&iter, "{sv}", &prop, &value)) {
            libdbusmenu_parse_properties(menu_item, prop, value, &is_section,
                                         &is_visible);
        }

        // now's a good time to recurse to the child of this item, we do it here
        // becaues it makes building a GMenu model with sections easier...
        libdbusmenu_parse_layout(child, menu_item, item);

        // menu item isn't visible, just skip trying to append it.
        if (!is_visible) goto skip_append;

        // if this child starts a section we need to create a new GMenu and
        // add subsequent items into it.
        //
        // the current menu node may have multiple sections, so if we encounter
        // a new one, we end our current section, add it to the menu we are
        // building, and create a new one.
        //
        // the final section, or this one, if is the only one, is appended after
        // all children within this loop are processed.
        if (is_section) {
            g_debug(
                "status_notifier_service.c:libdbusmenu_parse_layout() creating "
                "section");
            if (current_section) {
                g_debug(
                    "status_notifier_service.c:libdbusmenu_parse_layout() "
                    "appending section");
                g_menu_append_section(menu, NULL,
                                      G_MENU_MODEL(current_section));
                g_object_unref(current_section);
            }
            current_section = g_menu_new();
        } else {
            // we only need to attach a click action to non-section nodes.
            g_menu_item_set_action_and_target_value(
                menu_item, SNI_GRACTION_ITEM_CLICKED,
                g_variant_new("(si)", item->bus_name, child_id));
            // if we have a current section, append it there, else append it to
            // the main menu we are building.
            if (current_section) {
                g_menu_append_item(current_section, menu_item);
            } else {
                g_menu_append_item(menu, menu_item);
            }
        }

    skip_append:
        g_variant_unref(child_props);
        g_variant_unref(child);
        position++;
    }

    // append any pending section to our menu...
    if (current_section)
        g_menu_append_section(menu, NULL, G_MENU_MODEL(current_section));

    // if our parent_menu_item is null, this is the root iteration and we attach
    // our menu to the SNI
    if (parent_menu_item == NULL)
        item->menu_model = menu;
    else if (position > 0)
        // otherwise, this is a submenu iteration, if we wound up creating a
        // menu due to there being submenu items, attach this menu to our
        // parent.
        g_menu_item_set_submenu(parent_menu_item, G_MENU_MODEL(menu));
    else
        g_free(menu);
}
