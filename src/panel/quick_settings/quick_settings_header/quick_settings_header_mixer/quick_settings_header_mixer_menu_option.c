#include "quick_settings_header_mixer_menu_option.h"

#include <adwaita.h>
#include <gio/gio.h>

#include "../../../../services/wireplumber_service.h"
#include "../../quick_settings_menu_widget.h"
#include "glibconfig.h"
#include "gtk/gtkdropdown.h"
#include "gtk/gtkrevealer.h"

static GAppInfo *search_apps_by_app_id(const gchar *app_id) {
    GAppInfo *app_info = NULL;
    GList *apps = g_app_info_get_all();
    GList *l;
    g_debug("app_switcher_app_widget: search_apps_by_app_id: %s", app_id);
    for (l = apps; l != NULL; l = l->next) {
        GAppInfo *info = l->data;
        const gchar *id = g_app_info_get_id(info);
        const gchar *lower_id = g_utf8_strdown(id, -1);
        const gchar *lower_app_id = g_utf8_strdown(app_id, -1);
        if (g_strrstr(lower_id, lower_app_id)) {
            app_info = info;
            break;
        }
    }
    g_list_free(apps);
    return app_info;
}

enum signals { signals_n };

typedef struct _QuickSettingsHeaderMixerMenuOption {
    GObject parent_instance;
    WirePlumberServiceNodeHeader *node;
    GtkBox *container;
    GtkButton *button;
    GtkBox *button_contents;
    GtkLabel *node_name;
    GtkImage *icon;
    GtkImage *active_icon;
    GtkEventControllerMotion *ctlr;
    GtkScale *volume_scale;
    GtkLabel *link;
    GtkRevealer *revealer;
    GtkBox *revealer_content;
    GtkDropDown *streams_dropdown;
} QuickSettingsHeaderMixerMenuOption;
G_DEFINE_TYPE(QuickSettingsHeaderMixerMenuOption,
              quick_settings_header_mixer_menu_option, G_TYPE_OBJECT);

// event handler forward declare
static void on_wire_plumber_service_node_changed(
    WirePlumberService *wps, WirePlumberServiceNodeHeader *header,
    QuickSettingsHeaderMixerMenuOption *self);

// stub out empty dispose, finalize, class_init, and init methods for this
// GObject.
static void quick_settings_header_mixer_menu_option_dispose(GObject *gobject) {
    QuickSettingsHeaderMixerMenuOption *self =
        QUICK_SETTINGS_HEADER_MIXER_MENU_OPTION(gobject);

    // kill signals to wire_plumber_service
    g_signal_handlers_disconnect_by_func(wire_plumber_service_get_global(),
                                         on_wire_plumber_service_node_changed,
                                         self);
    g_debug(
        "quick_settings_header_mixer_menu_option.c:"
        "quick_settings_header_mixer_menu_option_dispose() called.");

    // Chain-up
    G_OBJECT_CLASS(quick_settings_header_mixer_menu_option_parent_class)
        ->dispose(gobject);
};

static void quick_settings_header_mixer_menu_option_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(quick_settings_header_mixer_menu_option_parent_class)
        ->finalize(gobject);
};

static void quick_settings_header_mixer_menu_option_class_init(
    QuickSettingsHeaderMixerMenuOptionClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = quick_settings_header_mixer_menu_option_dispose;
    object_class->finalize = quick_settings_header_mixer_menu_option_finalize;
};

static void on_option_click(GtkButton *button,
                            QuickSettingsHeaderMixerMenuOption *self) {
    g_debug(
        "quick_settings_header_mixer_menu_option.c:on_button_click() called.");
    gboolean revealed =
        gtk_revealer_get_reveal_child(GTK_REVEALER(self->revealer));
    gtk_revealer_set_reveal_child(self->revealer, !revealed);
    if (!revealed) {
        gtk_widget_grab_focus(GTK_WIDGET(self->revealer_content));
    }
}

static void on_scale_value_changed(GtkRange *range,
                                   QuickSettingsHeaderMixerMenuOption *self) {
    g_debug(
        "quick_settings_header_mixer_menu_option.c:on_scale_value_changed() "
        "called.");

    // get value of scale and set volume of node
    double value = gtk_range_get_value(range);
    wire_plumber_service_set_volume(wire_plumber_service_get_global(),
                                    (WirePlumberServiceNode *)self->node,
                                    value);
}

static void on_volume_scale_enter(GtkEventControllerMotion *ctlr, double x,
                                  double y,
                                  QuickSettingsHeaderMixerMenuOption *self) {
    g_debug(
        "quick_settings_header_mixer_menu_option.c:on_volume_scale_enter() "
        "called.");

    // pause node-changed events
    WirePlumberService *wp = wire_plumber_service_get_global();

    g_signal_handlers_block_by_func(
        wp, G_CALLBACK(on_wire_plumber_service_node_changed), self);

    // unblock gtk scale handler
    g_signal_handlers_unblock_by_func(GTK_RANGE(self->volume_scale),
                                      G_CALLBACK(on_scale_value_changed), self);
}

static void on_volume_scale_leave(GtkEventControllerMotion *ctlr, double x,
                                  double y,
                                  QuickSettingsHeaderMixerMenuOption *self) {
    g_debug(
        "quick_settings_header_mixer_menu_option.c:on_volume_scale_leave() "
        "called.");

    // pause node-changed events
    WirePlumberService *wp = wire_plumber_service_get_global();

    // unblock gtk scale handler
    g_signal_handlers_block_by_func(GTK_RANGE(self->volume_scale),
                                    G_CALLBACK(on_scale_value_changed), self);

    g_signal_handlers_unblock_by_func(
        wp, G_CALLBACK(on_wire_plumber_service_node_changed), self);
}

static void quick_settings_header_mixer_menu_option_init_layout(
    QuickSettingsHeaderMixerMenuOption *self) {
    g_debug(
        "quick_settings_header_mixer_menu_option.c:"
        "quick_settings_header_mixer_menu_option_init_layout() called.");

    // create main container
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->container),
                             "quick-settings-menu-option-mixer");

    // create main button
    self->button = GTK_BUTTON(gtk_button_new());

    // create button contents
    self->button_contents = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

    self->icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("audio-speakers-symbolic"));

    self->active_icon =
        GTK_IMAGE(gtk_image_new_from_icon_name("media-record-symbolic"));

    self->node_name = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(self->node_name, 0);
    gtk_label_set_max_width_chars(self->node_name, 120);
    gtk_label_set_ellipsize(self->node_name, PANGO_ELLIPSIZE_END);

    // append icon, node_name, active icon to button_contents
    gtk_box_append(self->button_contents, GTK_WIDGET(self->active_icon));
    gtk_box_append(self->button_contents, GTK_WIDGET(self->icon));
    gtk_box_append(self->button_contents, GTK_WIDGET(self->node_name));

    // create volume scale
    self->volume_scale = GTK_SCALE(
        gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 1.0, .5));
    gtk_widget_set_hexpand(GTK_WIDGET(self->volume_scale), true);

    // wire scale into handler
    g_signal_connect(self->volume_scale, "value-changed",
                     G_CALLBACK(on_scale_value_changed), self);

    // create motion controller for handling mouse in/out events on scale
    self->ctlr = GTK_EVENT_CONTROLLER_MOTION(gtk_event_controller_motion_new());
    gtk_widget_add_controller(GTK_WIDGET(self->volume_scale),
                              GTK_EVENT_CONTROLLER(self->ctlr));

    // wire into motion controller's enter event
    g_signal_connect(self->ctlr, "enter", G_CALLBACK(on_volume_scale_enter),
                     self);

    g_signal_connect(self->ctlr, "leave", G_CALLBACK(on_volume_scale_leave),
                     self);

    // add button_contents as button child
    gtk_button_set_child(self->button, GTK_WIDGET(self->button_contents));

    // add button to container
    gtk_box_append(self->container, GTK_WIDGET(self->button));

    // add revealer
    self->revealer = GTK_REVEALER(gtk_revealer_new());
    gtk_revealer_set_transition_type(self->revealer,
                                     GTK_REVEALER_TRANSITION_TYPE_SWING_DOWN);
    gtk_revealer_set_transition_duration(self->revealer, 350);

    self->revealer_content = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_can_focus(GTK_WIDGET(self->revealer_content), true);
    gtk_revealer_set_child(self->revealer, GTK_WIDGET(self->revealer_content));

    // append revealer to container
    gtk_box_append(self->container, GTK_WIDGET(self->revealer));

    g_signal_connect(self->button, "clicked", G_CALLBACK(on_option_click),
                     self);
};

static void on_widget_destroy(GtkWidget *widget,
                              QuickSettingsHeaderMixerMenuOption *self) {
    g_debug(
        "quick_settings_header_mixer_menu_option.c:on_widget_destroy() "
        "called.");
    g_object_unref(self);
}

static void quick_settings_header_mixer_menu_option_init(
    QuickSettingsHeaderMixerMenuOption *self) {
    quick_settings_header_mixer_menu_option_init_layout(self);
    // attach pointer to self to container's data
    g_object_set_data(G_OBJECT(self->container), "option", self);

    // tie our lifespan to the lifespan of our container
    g_signal_connect(self->container, "destroy", G_CALLBACK(on_widget_destroy),
                     self);
};

static void set_sink(QuickSettingsHeaderMixerMenuOption *self,
                     WirePlumberServiceNode *node) {
    g_debug("quick_settings_header_mixer_menu_option.c:set_sink() called.");

    gboolean is_default = false;
    WirePlumberService *wps = wire_plumber_service_get_global();
    WirePlumberServiceNode *default_sink =
        wire_plumber_service_get_default_sink(wps);
    if (default_sink) is_default = default_sink->id == node->id;

    gchar *icon =
        wire_plumber_service_map_sink_vol_icon(node->volume, node->mute);
    // set icon
    gtk_image_set_from_icon_name(self->icon, icon);

    // set name
    if (node->nick_name) {
        gtk_label_set_text(self->node_name, node->nick_name);
    } else {
        gtk_label_set_text(self->node_name, node->name);
    }

    if (is_default) {
        const gchar *name_label_text = gtk_label_get_text(self->node_name);
        gtk_label_set_text(self->node_name,
                           g_strdup_printf("*%s", name_label_text));
    }

    // set tooltip to node_name
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->button), node->name);

    // set scale to node's volume
    gtk_range_set_value(GTK_RANGE(self->volume_scale), node->volume);

    if (!gtk_widget_get_first_child(gtk_revealer_get_child(self->revealer)))
        gtk_box_append(self->revealer_content, GTK_WIDGET(self->volume_scale));

    if (node->state == WP_NODE_STATE_RUNNING) {
        gtk_widget_add_css_class(GTK_WIDGET(self->active_icon),
                                 "active-icon-activated");
    } else {
        gtk_widget_remove_css_class(GTK_WIDGET(self->active_icon),
                                    "active-icon-activated");
    }
}

static void set_source(QuickSettingsHeaderMixerMenuOption *self,
                       WirePlumberServiceNode *node) {
    g_debug("quick_settings_header_mixer_menu_option.c:set_source() called.");

    gboolean is_default = false;
    WirePlumberService *wps = wire_plumber_service_get_global();
    WirePlumberServiceNode *default_source =
        wire_plumber_service_get_default_source(wps);
    if (default_source) is_default = default_source->id == node->id;

    gchar *icon =
        wire_plumber_service_map_source_vol_icon(node->volume, node->mute);
    // set icon
    gtk_image_set_from_icon_name(self->icon, icon);

    // set name
    if (node->nick_name) {
        gtk_label_set_text(self->node_name, node->nick_name);
    } else {
        gtk_label_set_text(self->node_name, node->name);
    }

    if (is_default) {
        const gchar *name_label_text = gtk_label_get_text(self->node_name);
        gtk_label_set_text(self->node_name,
                           g_strdup_printf("*%s", name_label_text));
    }

    // set tooltip to node_name
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->button), node->name);

    // set scale to node's volume
    gtk_range_set_value(GTK_RANGE(self->volume_scale), node->volume);

    // set scale as revealer's content if revealer does not have a child
    if (!gtk_widget_get_first_child(gtk_revealer_get_child(self->revealer)))
        gtk_box_append(self->revealer_content, GTK_WIDGET(self->volume_scale));

    // check state and if running set active css on active_icon
    if (node->state == WP_NODE_STATE_RUNNING) {
        gtk_widget_add_css_class(GTK_WIDGET(self->active_icon),
                                 "active-icon-activated");
    } else {
        gtk_widget_remove_css_class(GTK_WIDGET(self->active_icon),
                                    "active-icon-activated");
    }
}

GtkButton *link_button_new(WirePlumberServiceNodeHeader *header) {
    WirePlumberServiceNode *node = (WirePlumberServiceNode *)header;

    g_debug(
        "quick_settings_header_mixer_menu_option.c:link_button_new() "
        "called.");

    // create button which reveals the link
    GtkButton *link_button = GTK_BUTTON(gtk_button_new());
    gtk_widget_set_tooltip_text(GTK_WIDGET(link_button), node->name);

    // create button content container
    GtkBox *button_content =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(link_button), "mixer-link");

    // create link icon, the hotspot icon looks like a chain link which
    // is good for this purpose.
    GtkImage *icon = GTK_IMAGE(
        gtk_image_new_from_icon_name("network-wireless-hotspot-symbolic"));

    // copy the name of the link
    char *name = NULL;
    if (node->nick_name) {
        name = g_strdup(node->nick_name);
    } else {
        name = g_strdup(node->name);
    }

    // add label of link name
    GtkLabel *label = GTK_LABEL(gtk_label_new(name));
    gtk_label_set_max_width_chars(label, 120);
    gtk_label_set_ellipsize(label, PANGO_ELLIPSIZE_END);

    // append icon and label to button contents and set contents as
    // button's child
    gtk_box_append(button_content, GTK_WIDGET(icon));
    gtk_box_append(button_content, GTK_WIDGET(label));
    gtk_button_set_child(link_button, GTK_WIDGET(button_content));

    return link_button;
}

static void set_stream_common(QuickSettingsHeaderMixerMenuOption *self,
                              WirePlumberServiceAudioStream *node) {
    // prefer icons from app info
    GAppInfo *app_info = search_apps_by_app_id(node->app_name);
    if (app_info) {
        GIcon *icon = g_app_info_get_icon(G_APP_INFO(app_info));
        // check and handle GFileIcon, set self->icon to a GtkImage
        if (G_IS_FILE_ICON(icon)) {
            g_debug("G_FILE_ICON");
        }
        // check and handle if GThemedIcon
        else if (G_IS_THEMED_ICON(icon)) {
            GdkDisplay *display = gdk_display_get_default();
            GtkIconTheme *theme = gtk_icon_theme_get_for_display(display);
            GtkIconPaintable *paintable = gtk_icon_theme_lookup_by_gicon(
                theme, icon, 64, 1, GTK_TEXT_DIR_RTL, 0);
            gtk_image_set_from_paintable(self->icon, GDK_PAINTABLE(paintable));
        }
        // check and handle if GLoadableIcon
        else if (G_IS_LOADABLE_ICON(icon)) {
            g_debug("G_LOADABLE_ICON");
        }
    } else
        gtk_image_set_from_icon_name(self->icon,
                                     "applications-multimedia-symbolic");

    // set name and tooltip based on stream direction
    if (node->type == WIRE_PLUMBER_SERVICE_TYPE_INPUT_AUDIO_STREAM) {
        gtk_label_set_text(
            self->node_name,
            g_strdup_printf("%s (%s)", node->media_name, "Input"));
        gtk_widget_set_tooltip_text(
            GTK_WIDGET(self->button),
            g_strdup_printf("%s: %s (%s)", node->app_name, node->media_name,
                            "Input"));
    } else {
        gtk_label_set_text(
            self->node_name,
            g_strdup_printf("%s (%s)", node->media_name, "Output"));
        gtk_widget_set_tooltip_text(
            GTK_WIDGET(self->button),
            g_strdup_printf("%s: %s (%s)", node->app_name, node->media_name,
                            "Output"));
    }

    if (node->state == WP_NODE_STATE_RUNNING) {
        gtk_widget_add_css_class(GTK_WIDGET(self->active_icon),
                                 "active-icon-activated");
    } else {
        gtk_widget_remove_css_class(GTK_WIDGET(self->active_icon),
                                    "active-icon-activated");
    }
}

static void on_input_stream_dropdown_activate(
    GtkDropDown *dropdown, GParamSpec *pspec,
    QuickSettingsHeaderMixerMenuOption *self) {
    g_debug(
        "quick_settings_header_mixer_menu_option.c:on_input_stream_dropdown_"
        "activate() called.");

    gint32 index = gtk_drop_down_get_selected(dropdown);

    // get list of all sources
    GPtrArray *sources =
        wire_plumber_service_get_sources(wire_plumber_service_get_global());

    // get the source at the index of the dropdown
    WirePlumberServiceNode *source = g_ptr_array_index(sources, index);

    // get the stream
    WirePlumberServiceAudioStream *stream =
        (WirePlumberServiceAudioStream *)self->node;

    // set the link
    wire_plumber_service_set_link(wire_plumber_service_get_global(),
                                  (WirePlumberServiceNodeHeader *)source,
                                  (WirePlumberServiceNodeHeader *)stream);
}

static void set_input_stream(QuickSettingsHeaderMixerMenuOption *self,
                             WirePlumberServiceAudioStream *node) {
    g_debug(
        "quick_settings_header_mixer_menu_option.c:set_output_stream() "
        "called.");

    guint32 linked_output = 0;
    gint32 linked_output_index = -1;

    // unparent revealer's content making this function idempotent.
    if (gtk_widget_get_first_child(GTK_WIDGET(self->revealer_content)))
        gtk_widget_unparent(
            gtk_widget_get_first_child(GTK_WIDGET(self->revealer_content)));

    // find links which reference this node
    GPtrArray *links =
        wire_plumber_service_get_links(wire_plumber_service_get_global());
    for (int i = 0; i < links->len; i++) {
        WirePlumberServiceLink *link = g_ptr_array_index(links, i);
        if (link->input_node == node->id) {
            linked_output = link->output_node;
        }
    }

    // get list of all sources
    GPtrArray *sources =
        wire_plumber_service_get_sources(wire_plumber_service_get_global());
    const char *source_names[sources->len + 1];  // +1 for null terminator.
    source_names[sources->len] = NULL;

    // iterate sources and add their names to list fed into drop down.
    for (int i = 0; i < sources->len; i++) {
        WirePlumberServiceNode *sink = g_ptr_array_index(sources, i);
        if (sink->nick_name)
            source_names[i] = sink->nick_name;
        else
            source_names[i] = sink->name;
        if (sink->id == linked_output) linked_output_index = i;
    }

    GtkBox *dropdown_contents =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(dropdown_contents), "mixer-link");

    // append icon
    GtkImage *link_icon = GTK_IMAGE(
        gtk_image_new_from_icon_name("network-wireless-hotspot-symbolic"));
    gtk_box_append(dropdown_contents, GTK_WIDGET(link_icon));

    // create dropdown button
    self->streams_dropdown =
        GTK_DROP_DOWN(gtk_drop_down_new_from_strings(source_names));
    if (linked_output_index != -1)
        gtk_drop_down_set_selected(self->streams_dropdown, linked_output_index);

    // wire up drop down's activate
    g_signal_connect(self->streams_dropdown, "notify::selected",
                     G_CALLBACK(on_input_stream_dropdown_activate), self);

    // append dropdown to revealer content
    gtk_box_append(dropdown_contents, GTK_WIDGET(self->streams_dropdown));

    // add dropdow to revealer's content
    gtk_box_append(self->revealer_content, GTK_WIDGET(dropdown_contents));

    set_stream_common(self, node);
}

static void on_output_stream_dropdown_activate(
    GtkDropDown *dropdown, GParamSpec *pspec,
    QuickSettingsHeaderMixerMenuOption *self) {
    g_debug(
        "quick_settings_header_mixer_menu_option.c:on_output_stream_"
        "dropdown_"
        "activate() called.");

    gint32 index = gtk_drop_down_get_selected(dropdown);

    // get list of all sinks
    GPtrArray *sinks =
        wire_plumber_service_get_sinks(wire_plumber_service_get_global());

    // get the sink at the index of the dropdown
    WirePlumberServiceNode *sink = g_ptr_array_index(sinks, index);

    // get the stream
    WirePlumberServiceAudioStream *stream =
        (WirePlumberServiceAudioStream *)self->node;

    // set the link
    wire_plumber_service_set_link(wire_plumber_service_get_global(),
                                  (WirePlumberServiceNodeHeader *)stream,
                                  (WirePlumberServiceNodeHeader *)sink);
}

static void set_output_stream(QuickSettingsHeaderMixerMenuOption *self,
                              WirePlumberServiceAudioStream *node) {
    g_debug(
        "quick_settings_header_mixer_menu_option.c:set_output_stream() "
        "called.");

    guint32 linked_input = 0;
    gint32 linked_input_index = -1;

    if (gtk_widget_get_first_child(GTK_WIDGET(self->revealer_content)))
        gtk_widget_unparent(
            gtk_widget_get_first_child(GTK_WIDGET(self->revealer_content)));

    // find links which reference this node
    GPtrArray *links =
        wire_plumber_service_get_links(wire_plumber_service_get_global());
    for (int i = 0; i < links->len; i++) {
        WirePlumberServiceLink *link = g_ptr_array_index(links, i);
        if (link->output_node == node->id) {
            linked_input = link->input_node;
        }
    }

    // get list of all sinks that we may link to this output.
    GPtrArray *sinks =
        wire_plumber_service_get_sinks(wire_plumber_service_get_global());
    const char *sink_names[sinks->len + 1];  // +1 for null terminator.
    sink_names[sinks->len] = NULL;

    for (int i = 0; i < sinks->len; i++) {
        WirePlumberServiceNode *sink = g_ptr_array_index(sinks, i);
        if (sink->nick_name)
            sink_names[i] = sink->nick_name;
        else
            sink_names[i] = sink->name;
        if (sink->id == linked_input) linked_input_index = i;
    }

    GtkBox *dropdown_contents =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(dropdown_contents), "mixer-link");

    // append icon
    GtkImage *link_icon = GTK_IMAGE(
        gtk_image_new_from_icon_name("network-wireless-hotspot-symbolic"));
    gtk_box_append(dropdown_contents, GTK_WIDGET(link_icon));

    // create dropdown button
    self->streams_dropdown =
        GTK_DROP_DOWN(gtk_drop_down_new_from_strings(sink_names));
    if (linked_input_index != -1)
        gtk_drop_down_set_selected(self->streams_dropdown, linked_input_index);

    // wire into dropdown activate
    g_signal_connect(self->streams_dropdown, "notify::selected",
                     G_CALLBACK(on_output_stream_dropdown_activate), self);

    // append dropdown to revealer content
    gtk_box_append(dropdown_contents, GTK_WIDGET(self->streams_dropdown));

    // add dropdow to revealer's content
    gtk_box_append(self->revealer_content, GTK_WIDGET(dropdown_contents));

    set_stream_common(self, node);
}

static void on_wire_plumber_service_node_changed(
    WirePlumberService *wps, WirePlumberServiceNodeHeader *header,
    QuickSettingsHeaderMixerMenuOption *self) {
    g_debug(
        "quick_settings_header_mixer_menu_option.c:on_wire_plumber_node_"
        "changed_event() called.");

    if (self->node->id != header->id) return;

    self->node = header;

    switch (self->node->type) {
        case WIRE_PLUMBER_SERVICE_TYPE_SINK:
            set_sink(self, (WirePlumberServiceNode *)header);
            break;
        case WIRE_PLUMBER_SERVICE_TYPE_SOURCE:
            set_source(self, (WirePlumberServiceNode *)header);
            break;
        case WIRE_PLUMBER_SERVICE_TYPE_INPUT_AUDIO_STREAM:
            set_input_stream(self, (WirePlumberServiceAudioStream *)header);
            break;
        case WIRE_PLUMBER_SERVICE_TYPE_OUTPUT_AUDIO_STREAM:
            set_output_stream(self, (WirePlumberServiceAudioStream *)header);
            break;
        default:
            break;
    }
}

void quick_settings_header_mixer_menu_option_set_node(
    QuickSettingsHeaderMixerMenuOption *self,
    WirePlumberServiceNodeHeader *header) {
    g_debug(
        "quick_settings_header_mixer_menu_option.c:"
        "quick_settings_header_mixer_menu_option_set_node() called.");

    self->node = header;

    WirePlumberService *wps = wire_plumber_service_get_global();

    on_wire_plumber_service_node_changed(wps, header, self);

    g_signal_connect(wps, "node-changed",
                     G_CALLBACK(on_wire_plumber_service_node_changed), self);
}

GtkWidget *quick_settings_header_mixer_menu_option_get_widget(
    QuickSettingsHeaderMixerMenuOption *self) {
    return GTK_WIDGET(self->container);
}

WirePlumberServiceNodeHeader *quick_settings_header_mixer_menu_option_get_node(
    QuickSettingsHeaderMixerMenuOption *self) {
    return self->node;
}
