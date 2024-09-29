#include <adwaita.h>
#include <gio/gio.h>

#include "libdbus_menu_consts.h"
#include "status_notifier_service.h"

void libdbusmenu_parse_properties(GMenuItem *item, gchar *prop,
                                  GVariant *value) {
    if (g_strcmp0(prop, DBUSMENU_MENUITEM_PROP_TYPE) == 0) {
        // Type: #G_VARIANT_TYPE_STRING.
        g_debug(
            "status_notifier_service.c:libdbusmenu_parse_properties() setting "
            "label to %s",
            g_variant_get_string(value, NULL));
    }
    if (g_strcmp0(prop, DBUSMENU_MENUITEM_PROP_VISIBLE) == 0) {
        // Type: #G_VARIANT_TYPE_BOOLEAN.
        g_debug(
            "status_notifier_service.c:libdbusmenu_parse_properties() setting "
            "visible to %s",
            g_variant_get_boolean(value) ? "true" : "false");
    }
    if (g_strcmp0(prop, DBUSMENU_MENUITEM_PROP_ENABLED) == 0) {
        // Type: #G_VARIANT_TYPE_BOOLEAN.
        g_debug(
            "status_notifier_service.c:libdbusmenu_parse_properties() setting "
            "enabled to %s",
            g_variant_get_boolean(value) ? "true" : "false");
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
    if (g_strcmp0(prop, DBUSMENU_MENUITEM_PROP_ICON_NAME) == 0) {
        // Type: #G_VARIANT_TYPE_STRING.
        g_debug(
            "status_notifier_service.c:libdbusmenu_parse_properties() setting "
            "icon name to %s",
            g_variant_get_string(value, NULL));
    }
    if (g_strcmp0(prop, DBUSMENU_MENUITEM_PROP_ICON_DATA) == 0) {
        // Type: #G_VARIANT_TYPE_VARIANT.
        g_debug(
            "status_notifier_service.c:libdbusmenu_parse_properties() setting "
            "icon data to (%s)",
            g_variant_get_type_string(value));
    }
    if (g_strcmp0(prop, DBUSMENU_MENUITEM_PROP_ACCESSIBLE_DESC) == 0) {
        // Type: #G_VARIANT_TYPE_STRING.
        g_debug(
            "status_notifier_service.c:libdbusmenu_parse_properties() setting "
            "accessible description to %s",
            g_variant_get_string(value, NULL));
    }
    if (g_strcmp0(prop, DBUSMENU_MENUITEM_PROP_TOGGLE_TYPE) == 0) {
        // Type: #G_VARIANT_TYPE_STRING.
        g_debug(
            "status_notifier_service.c:libdbusmenu_parse_properties() setting "
            "toggle type to %s",
            g_variant_get_string(value, NULL));
    }
    if (g_strcmp0(prop, DBUSMENU_MENUITEM_PROP_TOGGLE_STATE) == 0) {
        // Type: #G_VARIANT_TYPE_INT32.
        g_debug(
            "status_notifier_service.c:libdbusmenu_parse_properties() setting "
            "toggle state to %d",
            g_variant_get_int32(value));
    }
    if (g_strcmp0(prop, DBUSMENU_MENUITEM_PROP_SHORTCUT) == 0) {
        // Type: #G_VARIANT_TYPE_ARRAY.
        g_debug(
            "status_notifier_service.c:libdbusmenu_parse_properties() setting "
            "shortcut to %s",
            g_variant_get_string(value, NULL));
    }
    if (g_strcmp0(prop, DBUSMENU_MENUITEM_PROP_CHILD_DISPLAY) == 0) {
        // Type: #G_VARIANT_TYPE_STRING.
        g_debug(
            "status_notifier_service.c:libdbusmenu_parse_properties() setting "
            "child display to %s",
            g_variant_get_string(value, NULL));
    }
    if (g_strcmp0(prop, DBUSMENU_MENUITEM_PROP_DISPOSITION) == 0) {
        // Type: #G_VARIANT_TYPE_STRING.
        g_debug(
            "status_notifier_service.c:libdbusmenu_parse_properties() setting "
            "disposition to %s",
            g_variant_get_string(value, NULL));
    }
}

// A recursive algorithm for parsing a root com.canonical.Menu structure.
// The alogirthm works through the tree, turning Layout structures into GMenu
// and GMenuItems.
//
// When the function returns item->menu_model will be initalized if a menu has
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
    // If we don't have a parent menut, this is the root iteration of a layout
    // parse.
    //
    // If the sni already has a menu, remove everything from it and update it,
    // else, create a new menu.
    GMenu *menu = NULL;
    if (!parent_menu_item) {
        if (item->menu_model) {
            g_menu_remove_all(item->menu_model);
            menu = G_MENU(item->menu_model);
        } else {
            menu = g_menu_new();
        }
    } else {
        menu = g_menu_new();
    }

    GVariantIter children;
    GVariant *children_variant;

    children_variant = g_variant_get_child_value(layout, 2);
    g_variant_iter_init(&children, children_variant);

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

        // TODO: Handle Sections correctly...

        // create a new menu item for this child and set its properties
        GMenuItem *menu_item = g_menu_item_new(
            NULL, "app.status_notifier_service-menu_item_clicked");

        g_menu_item_set_action_and_target_value(
            menu_item, SNI_GRACTION_ITEM_CLICKED,
            g_variant_new("(si)", item->bus_name, child_id));

        g_variant_iter_init(&iter, child_props);
        while (g_variant_iter_loop(&iter, "{sv}", &prop, &value)) {
            libdbusmenu_parse_properties(menu_item, prop, value);
        }

        // recurse to child menu...
        libdbusmenu_parse_layout(child, menu_item, item);

        // add our menu item to our parent menu
        g_menu_append_item(menu, menu_item);

        g_variant_unref(child_props);

        g_variant_unref(child);
        position++;
    }

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
